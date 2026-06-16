#ifndef __SENSOR_SERVICE_H__
#define __SENSOR_SERVICE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SENSOR_IMU_CALIBRATION_SAMPLES 20
#define SENSOR_TEMPERATURE_RESET_THRESHOLD 5

typedef struct {
    int32_t temperature_milli_c;
    OPERATE_RET temperature_status;
    uint32_t temperature_sample_count;
    uint32_t temperature_failure_count;
    uint32_t temperature_reset_count;
    uint8_t temperature_consecutive_failures;
    uint8_t temperature_error_stage;
    bool temperature_fresh;
    bool temperature_valid;

    int16_t acc_raw[3];
    int16_t gyro_raw[3];
    int32_t acc_milli_g[3];
    int32_t gyro_milli_dps[3];
    OPERATE_RET imu_status;
    uint32_t imu_sample_count;
    uint8_t imu_address;
    uint8_t imu_calibration_samples;
    bool imu_calibrated;
    bool imu_valid;
} SENSOR_DATA_T;

OPERATE_RET sensor_service_start(void);
OPERATE_RET sensor_service_get_data(SENSOR_DATA_T *data);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_SERVICE_H__ */
