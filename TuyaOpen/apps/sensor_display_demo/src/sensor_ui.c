#include <stdio.h>

#include "tal_api.h"
#include "lvgl.h"
#include "m117b.h"
#include "sensor_service.h"
#include "sensor_ui.h"
#include "tuya_lvgl.h"

static lv_obj_t *s_temperature_label = NULL;
static lv_obj_t *s_temperature_status_label = NULL;
static lv_obj_t *s_imu_status_label = NULL;
static lv_obj_t *s_gyro_labels[3] = {NULL};
static lv_obj_t *s_acc_label = NULL;
static lv_obj_t *s_sample_label = NULL;

static void __format_fixed(char *buffer, size_t size, int32_t value, uint32_t divisor, uint32_t fraction_divisor)
{
    uint32_t absolute_value = value < 0 ? (uint32_t)(-value) : (uint32_t)value;

    snprintf(buffer, size, "%s%lu.%02lu",
             value < 0 ? "-" : "",
             (unsigned long)(absolute_value / divisor),
             (unsigned long)((absolute_value % divisor) / fraction_divisor));
}

static void __sensor_timer_cb(lv_timer_t *timer)
{
    SENSOR_DATA_T data = {0};
    char value_x[16];
    char value_y[16];
    char value_z[16];
    int32_t absolute_temperature;
    const char *sign;
    int32_t temperature;
    int32_t fraction;

    (void)timer;

    if (sensor_service_get_data(&data) != OPRT_OK) {
        return;
    }

    if (data.temperature_valid) {
        sign = data.temperature_milli_c < 0 ? "-" : "";
        absolute_temperature = data.temperature_milli_c < 0 ? -data.temperature_milli_c : data.temperature_milli_c;
        temperature = absolute_temperature / 1000;
        fraction = absolute_temperature % 1000;

        lv_label_set_text_fmt(s_temperature_label, "%s%ld.%02ld C", sign, (long)temperature, (long)(fraction / 10));
        if (data.temperature_fresh) {
            lv_label_set_text(s_temperature_status_label, "M117B: OK");
            lv_obj_set_style_text_color(s_temperature_status_label, lv_color_hex(0x00ff66), 0);
        } else if (data.temperature_status == OPRT_OK) {
            lv_label_set_text(s_temperature_status_label, "M117B: WAIT");
            lv_obj_set_style_text_color(s_temperature_status_label, lv_color_hex(0xffd166), 0);
        } else if (data.temperature_consecutive_failures > 0 &&
                   data.temperature_consecutive_failures < SENSOR_TEMPERATURE_RESET_THRESHOLD) {
            lv_label_set_text_fmt(s_temperature_status_label, "M117B: %s %u/%u",
                                  m117b_get_stage_name((M117B_STAGE_E)data.temperature_error_stage),
                                  (unsigned int)data.temperature_consecutive_failures,
                                  (unsigned int)SENSOR_TEMPERATURE_RESET_THRESHOLD);
            lv_obj_set_style_text_color(s_temperature_status_label, lv_color_hex(0xffd166), 0);
        } else {
            lv_label_set_text_fmt(s_temperature_status_label, "M117B: ERR %s",
                                  m117b_get_stage_name((M117B_STAGE_E)data.temperature_error_stage));
            lv_obj_set_style_text_color(s_temperature_status_label, lv_color_hex(0xff5050), 0);
        }
    } else {
        lv_label_set_text(s_temperature_label, "--.-- C");
        if (data.temperature_status == OPRT_OK) {
            lv_label_set_text(s_temperature_status_label, "M117B: STARTING");
            lv_obj_set_style_text_color(s_temperature_status_label, lv_color_hex(0x50a0ff), 0);
        } else {
            lv_label_set_text_fmt(s_temperature_status_label, "M117B: ERR %s",
                                  m117b_get_stage_name((M117B_STAGE_E)data.temperature_error_stage));
            lv_obj_set_style_text_color(s_temperature_status_label, lv_color_hex(0xff5050), 0);
        }
    }

    if (data.imu_valid && data.imu_calibrated) {
        lv_label_set_text_fmt(s_imu_status_label, "SH3001: OK @ 0x%02X", data.imu_address);
        lv_obj_set_style_text_color(s_imu_status_label, lv_color_hex(0x00ff66), 0);
        __format_fixed(value_x, sizeof(value_x), data.gyro_milli_dps[0], 1000, 10);
        __format_fixed(value_y, sizeof(value_y), data.gyro_milli_dps[1], 1000, 10);
        __format_fixed(value_z, sizeof(value_z), data.gyro_milli_dps[2], 1000, 10);
        lv_label_set_text_fmt(s_gyro_labels[0], "GYRO X: %s dps", value_x);
        lv_label_set_text_fmt(s_gyro_labels[1], "GYRO Y: %s dps", value_y);
        lv_label_set_text_fmt(s_gyro_labels[2], "GYRO Z: %s dps", value_z);

        __format_fixed(value_x, sizeof(value_x), data.acc_milli_g[0], 1000, 10);
        __format_fixed(value_y, sizeof(value_y), data.acc_milli_g[1], 1000, 10);
        __format_fixed(value_z, sizeof(value_z), data.acc_milli_g[2], 1000, 10);
        lv_label_set_text_fmt(s_acc_label, "ACC g: %s %s %s", value_x, value_y, value_z);
    } else if (data.imu_valid) {
        lv_label_set_text_fmt(s_imu_status_label, "SH3001: CAL %u/%u",
                              (unsigned int)data.imu_calibration_samples,
                              (unsigned int)SENSOR_IMU_CALIBRATION_SAMPLES);
        lv_obj_set_style_text_color(s_imu_status_label, lv_color_hex(0xffd166), 0);
        lv_label_set_text(s_gyro_labels[0], "Keep board still");
        lv_label_set_text(s_gyro_labels[1], "GYRO Y: -----");
        lv_label_set_text(s_gyro_labels[2], "GYRO Z: -----");
        lv_label_set_text(s_acc_label, "ACC g: calibrating");
    } else {
        lv_label_set_text_fmt(s_imu_status_label, "SH3001: ERROR %d", data.imu_status);
        lv_obj_set_style_text_color(s_imu_status_label, lv_color_hex(0xff5050), 0);
        lv_label_set_text(s_gyro_labels[0], "GYRO X: -----");
        lv_label_set_text(s_gyro_labels[1], "GYRO Y: -----");
        lv_label_set_text(s_gyro_labels[2], "GYRO Z: -----");
        lv_label_set_text(s_acc_label, "ACC g: -----  -----  -----");
    }

    lv_label_set_text_fmt(s_sample_label, "T:%lu F:%lu R:%lu I:%lu",
                          (unsigned long)data.temperature_sample_count,
                          (unsigned long)data.temperature_failure_count,
                          (unsigned long)data.temperature_reset_count,
                          (unsigned long)data.imu_sample_count);
}

OPERATE_RET sensor_ui_init(void)
{
    tuya_lvgl_mutex_lock();

    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Sensor Display Demo");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    s_temperature_label = lv_label_create(screen);
    lv_label_set_text(s_temperature_label, "--.-- C");
    lv_obj_set_style_text_color(s_temperature_label, lv_color_hex(0xffd166), 0);
    lv_obj_align(s_temperature_label, LV_ALIGN_TOP_MID, 0, 34);

    s_temperature_status_label = lv_label_create(screen);
    lv_label_set_text(s_temperature_status_label, "M117B: STARTING");
    lv_obj_set_style_text_color(s_temperature_status_label, lv_color_hex(0x50a0ff), 0);
    lv_obj_align(s_temperature_status_label, LV_ALIGN_TOP_MID, 0, 58);

    s_imu_status_label = lv_label_create(screen);
    lv_label_set_text(s_imu_status_label, "SH3001: STARTING");
    lv_obj_set_style_text_color(s_imu_status_label, lv_color_hex(0x50a0ff), 0);
    lv_obj_align(s_imu_status_label, LV_ALIGN_TOP_MID, 0, 82);

    for (uint32_t i = 0; i < 3; i++) {
        s_gyro_labels[i] = lv_label_create(screen);
        lv_obj_set_style_text_color(s_gyro_labels[i], lv_color_hex(0x70d6ff), 0);
        lv_obj_align(s_gyro_labels[i], LV_ALIGN_TOP_MID, 0, 108 + (i * 24));
    }
    lv_label_set_text(s_gyro_labels[0], "GYRO X: -----");
    lv_label_set_text(s_gyro_labels[1], "GYRO Y: -----");
    lv_label_set_text(s_gyro_labels[2], "GYRO Z: -----");

    s_acc_label = lv_label_create(screen);
    lv_label_set_text(s_acc_label, "ACC g: -----  -----  -----");
    lv_obj_set_style_text_color(s_acc_label, lv_color_hex(0xcdb4db), 0);
    lv_obj_align(s_acc_label, LV_ALIGN_TOP_MID, 0, 180);

    s_sample_label = lv_label_create(screen);
    lv_label_set_text(s_sample_label, "T:0 F:0 R:0 I:0");
    lv_obj_set_style_text_color(s_sample_label, lv_color_hex(0xa0a0a0), 0);
    lv_obj_align(s_sample_label, LV_ALIGN_TOP_MID, 0, 204);

    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, "I2C: P42 SCL / P43 SDA");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xa0a0a0), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);

    lv_timer_create(__sensor_timer_cb, 200, NULL);

    tuya_lvgl_mutex_unlock();

    PR_NOTICE("sensor display UI init success");

    return OPRT_OK;
}
