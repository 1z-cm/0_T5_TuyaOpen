#include <string.h>

#include "tal_api.h"

#include "m117b.h"
#include "sensor_service.h"
#include "sh3001.h"

#define SENSOR_LOOP_INTERVAL_MS 100
#define TEMPERATURE_SAMPLE_TICKS 10
#define SENSOR_RECOVERY_TICKS 50
#define SENSOR_READ_ATTEMPT_COUNT 4
#define SH3001_ACC_LSB_PER_G 2048
#define SH3001_GYRO_LSB_PER_DPS_X10 164

static const uint32_t s_temperature_retry_delays_ms[SENSOR_READ_ATTEMPT_COUNT - 1] = {20, 50, 100};

static THREAD_HANDLE s_sensor_thread = NULL;
static MUTEX_HANDLE s_data_mutex = NULL;
static SENSOR_DATA_T s_sensor_data = {
    .temperature_status = OPRT_COM_ERROR,
    .imu_status = OPRT_COM_ERROR,
};

static int32_t s_gyro_bias[3] = {0};
static int32_t s_gyro_calibration_sum[3] = {0};
static uint8_t s_gyro_calibration_count = 0;

static void __reset_imu_calibration(void)
{
    memset(s_gyro_bias, 0, sizeof(s_gyro_bias));
    memset(s_gyro_calibration_sum, 0, sizeof(s_gyro_calibration_sum));
    s_gyro_calibration_count = 0;
}

static void __update_imu_data(const SH3001_DATA_T *imu_data)
{
    uint32_t i;

    memcpy(s_sensor_data.acc_raw, imu_data->acc, sizeof(imu_data->acc));
    memcpy(s_sensor_data.gyro_raw, imu_data->gyro, sizeof(imu_data->gyro));

    for (i = 0; i < 3; i++) {
        s_sensor_data.acc_milli_g[i] =
            ((int32_t)imu_data->acc[i] * 1000) / SH3001_ACC_LSB_PER_G;
    }

    if (s_gyro_calibration_count < SENSOR_IMU_CALIBRATION_SAMPLES) {
        for (i = 0; i < 3; i++) {
            s_gyro_calibration_sum[i] += imu_data->gyro[i];
        }
        s_gyro_calibration_count++;
        s_sensor_data.imu_calibration_samples = s_gyro_calibration_count;

        if (s_gyro_calibration_count == SENSOR_IMU_CALIBRATION_SAMPLES) {
            for (i = 0; i < 3; i++) {
                s_gyro_bias[i] = s_gyro_calibration_sum[i] / SENSOR_IMU_CALIBRATION_SAMPLES;
            }
            s_sensor_data.imu_calibrated = true;
            PR_NOTICE("SH3001 gyro calibrated: bias X=%ld Y=%ld Z=%ld",
                      (long)s_gyro_bias[0], (long)s_gyro_bias[1], (long)s_gyro_bias[2]);
        }
    }

    if (s_sensor_data.imu_calibrated) {
        for (i = 0; i < 3; i++) {
            int32_t corrected = (int32_t)imu_data->gyro[i] - s_gyro_bias[i];
            s_sensor_data.gyro_milli_dps[i] =
                (corrected * 10000) / SH3001_GYRO_LSB_PER_DPS_X10;
        }
    }
}

static OPERATE_RET __read_temperature(int32_t *temperature_milli_c)
{
    OPERATE_RET ret;
    uint8_t retry;

    for (retry = 0; retry < SENSOR_READ_ATTEMPT_COUNT; retry++) {
        ret = m117b_read_temperature(temperature_milli_c);
        if (ret == OPRT_OK) {
            return OPRT_OK;
        }

        if ((retry + 1) < SENSOR_READ_ATTEMPT_COUNT) {
            tal_system_sleep(s_temperature_retry_delays_ms[retry]);
        }
    }

    return ret;
}

static void __sensor_thread(void *arg)
{
    OPERATE_RET temp_ret;
    OPERATE_RET imu_ret;
    int32_t temperature_milli_c = 0;
    SH3001_DATA_T imu_data = {0};
    uint32_t loop_count = 0;
    uint32_t temp_recovery_ticks = 0;
    uint32_t imu_recovery_ticks = 0;
    uint8_t temp_consecutive_failures;
    bool temp_ready;
    bool imu_ready;

    (void)arg;

    temp_ret = m117b_init();
    temp_ready = (temp_ret == OPRT_OK);
    imu_ret = sh3001_init();
    imu_ready = (imu_ret == OPRT_OK);
    __reset_imu_calibration();

    tal_mutex_lock(s_data_mutex);
    s_sensor_data.temperature_status = temp_ret;
    s_sensor_data.imu_status = imu_ret;
    s_sensor_data.imu_address = sh3001_get_address();
    tal_mutex_unlock(s_data_mutex);

    while (1) {
        loop_count++;

        if (!imu_ready) {
            if (++imu_recovery_ticks >= SENSOR_RECOVERY_TICKS) {
                imu_recovery_ticks = 0;
                PR_NOTICE("SH3001 recovery attempt");
                imu_ret = sh3001_init();
                imu_ready = (imu_ret == OPRT_OK);
                if (imu_ready) {
                    __reset_imu_calibration();
                }

                tal_mutex_lock(s_data_mutex);
                s_sensor_data.imu_status = imu_ret;
                s_sensor_data.imu_address = sh3001_get_address();
                s_sensor_data.imu_calibration_samples = 0;
                s_sensor_data.imu_calibrated = false;
                s_sensor_data.imu_valid = false;
                tal_mutex_unlock(s_data_mutex);
            }
        } else {
            imu_ret = sh3001_read_data(&imu_data);
            tal_mutex_lock(s_data_mutex);
            s_sensor_data.imu_status = imu_ret;
            if (imu_ret == OPRT_OK) {
                __update_imu_data(&imu_data);
                s_sensor_data.imu_sample_count++;
                s_sensor_data.imu_valid = true;
            } else {
                s_sensor_data.imu_calibration_samples = 0;
                s_sensor_data.imu_calibrated = false;
                s_sensor_data.imu_valid = false;
                imu_ready = false;
                imu_recovery_ticks = 0;
            }
            tal_mutex_unlock(s_data_mutex);

            if (imu_ret != OPRT_OK) {
                PR_ERR("SH3001 read failed: %d", imu_ret);
            }
        }

        if (!temp_ready) {
            if (++temp_recovery_ticks >= SENSOR_RECOVERY_TICKS) {
                temp_recovery_ticks = 0;
                PR_NOTICE("M117B recovery attempt");
                temp_ret = m117b_init();
                temp_ready = (temp_ret == OPRT_OK);

                tal_mutex_lock(s_data_mutex);
                s_sensor_data.temperature_status = temp_ret;
                s_sensor_data.temperature_error_stage = (uint8_t)m117b_get_last_error_stage();
                s_sensor_data.temperature_fresh = false;
                tal_mutex_unlock(s_data_mutex);
            }
        } else if ((loop_count % TEMPERATURE_SAMPLE_TICKS) == 0) {
            temp_ret = __read_temperature(&temperature_milli_c);
            tal_mutex_lock(s_data_mutex);
            s_sensor_data.temperature_status = temp_ret;
            s_sensor_data.temperature_error_stage = (uint8_t)m117b_get_last_error_stage();
            if (temp_ret == OPRT_OK) {
                s_sensor_data.temperature_milli_c = temperature_milli_c;
                s_sensor_data.temperature_sample_count++;
                s_sensor_data.temperature_consecutive_failures = 0;
                s_sensor_data.temperature_fresh = true;
                s_sensor_data.temperature_valid = true;
            } else {
                s_sensor_data.temperature_failure_count++;
                if (s_sensor_data.temperature_consecutive_failures < 255) {
                    s_sensor_data.temperature_consecutive_failures++;
                }
                s_sensor_data.temperature_fresh = false;
            }
            temp_consecutive_failures = s_sensor_data.temperature_consecutive_failures;
            tal_mutex_unlock(s_data_mutex);

            if (temp_ret != OPRT_OK) {
                PR_ERR("M117B sample failed: stage=%s, ret=%d, consecutive=%u/%u",
                       m117b_get_stage_name(m117b_get_last_error_stage()), temp_ret,
                       (unsigned int)temp_consecutive_failures,
                       (unsigned int)SENSOR_TEMPERATURE_RESET_THRESHOLD);

                if (temp_consecutive_failures >= SENSOR_TEMPERATURE_RESET_THRESHOLD) {
                    PR_NOTICE("M117B reset after consecutive sample failures");
                    temp_ret = m117b_init();
                    temp_ready = (temp_ret == OPRT_OK);

                    tal_mutex_lock(s_data_mutex);
                    s_sensor_data.temperature_status = temp_ret;
                    s_sensor_data.temperature_error_stage = (uint8_t)m117b_get_last_error_stage();
                    s_sensor_data.temperature_reset_count++;
                    s_sensor_data.temperature_fresh = false;
                    if (temp_ready) {
                        s_sensor_data.temperature_consecutive_failures = 0;
                    }
                    tal_mutex_unlock(s_data_mutex);

                    if (!temp_ready) {
                        temp_recovery_ticks = 0;
                    }
                }
            }
        }

        tal_system_sleep(SENSOR_LOOP_INTERVAL_MS);
    }
}

OPERATE_RET sensor_service_start(void)
{
    OPERATE_RET ret;
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 4096,
        .priority = THREAD_PRIO_3,
        .thrdname = "sensor_worker",
    };

    if (s_sensor_thread != NULL) {
        return OPRT_OK;
    }

    ret = tal_mutex_create_init(&s_data_mutex);
    if (ret != OPRT_OK) {
        return ret;
    }

    ret = tal_thread_create_and_start(&s_sensor_thread, NULL, NULL, __sensor_thread, NULL, &thread_cfg);
    if (ret != OPRT_OK) {
        tal_mutex_release(s_data_mutex);
        s_data_mutex = NULL;
        return ret;
    }

    return OPRT_OK;
}

OPERATE_RET sensor_service_get_data(SENSOR_DATA_T *data)
{
    if (data == NULL || s_data_mutex == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_data_mutex);
    memcpy(data, &s_sensor_data, sizeof(*data));
    tal_mutex_unlock(s_data_mutex);

    return OPRT_OK;
}
