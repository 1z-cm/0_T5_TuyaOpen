#include "tal_api.h"

#include "m117b.h"
#include "sensor_i2c_bus.h"

#define M117B_I2C_ADDRESS       0x45
#define M117B_RESET_DELAY_MS    5
#define M117B_BUS_IDLE_DELAY_MS 5
#define M117B_CONVERT_DELAY_MS  20
#define M117B_CRC_POLYNOMIAL    0x31
#define M117B_CRC_INITIAL_VALUE 0xFF

static M117B_STAGE_E s_last_error_stage = M117B_STAGE_NONE;

static uint8_t __m117b_crc8(const uint8_t *data, uint32_t size)
{
    uint8_t crc = M117B_CRC_INITIAL_VALUE;

    for (uint32_t byte = 0; byte < size; byte++) {
        crc ^= data[byte];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ M117B_CRC_POLYNOMIAL) : (uint8_t)(crc << 1);
        }
    }

    return crc;
}

static OPERATE_RET __m117b_write_command(uint8_t command_msb, uint8_t command_lsb)
{
    uint8_t command[2] = {command_msb, command_lsb};
    OPERATE_RET ret;

    tal_system_sleep(M117B_BUS_IDLE_DELAY_MS);
    ret = sensor_i2c_m117b_send(M117B_I2C_ADDRESS, command, sizeof(command));
    if (ret != OPRT_OK) {
        sensor_i2c_bus_recover();
    }
    return ret;
}

OPERATE_RET m117b_init(void)
{
    OPERATE_RET ret = sensor_i2c_bus_init();
    if (ret != OPRT_OK) {
        return ret;
    }

    ret = __m117b_write_command(0x30, 0xA2);
    if (ret != OPRT_OK) {
        s_last_error_stage = M117B_STAGE_RESET;
        PR_ERR("M117B reset failed at address 0x%02X: %d", M117B_I2C_ADDRESS, ret);
        return ret;
    }

    tal_system_sleep(M117B_RESET_DELAY_MS);
    s_last_error_stage = M117B_STAGE_NONE;
    PR_NOTICE("M117B detected at address 0x%02X", M117B_I2C_ADDRESS);

    return OPRT_OK;
}

OPERATE_RET m117b_read_temperature(int32_t *temperature_milli_c)
{
    uint8_t data[3] = {0};
    OPERATE_RET ret;
    int16_t raw_temperature;

    if (temperature_milli_c == NULL) {
        return OPRT_INVALID_PARM;
    }

    ret = __m117b_write_command(0xCC, 0x44);
    if (ret != OPRT_OK) {
        s_last_error_stage = M117B_STAGE_CONVERT;
        return ret;
    }

    tal_system_sleep(M117B_CONVERT_DELAY_MS);

    ret = sensor_i2c_m117b_receive(M117B_I2C_ADDRESS, data, sizeof(data));
    if (ret != OPRT_OK) {
        s_last_error_stage = M117B_STAGE_READ;
        sensor_i2c_bus_recover();
        return ret;
    }

    if (__m117b_crc8(data, 2) != data[2]) {
        s_last_error_stage = M117B_STAGE_CRC;
        PR_ERR("M117B CRC failed: data=%02X %02X crc=%02X", data[0], data[1], data[2]);
        return OPRT_CRC32_FAILED;
    }

    raw_temperature = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
    *temperature_milli_c = 40000 + ((int32_t)raw_temperature * 1000) / 256;
    s_last_error_stage = M117B_STAGE_NONE;

    PR_DEBUG("M117B raw=%02X %02X, temperature=%ld.%03ld C", data[0], data[1],
             (long)(*temperature_milli_c / 1000), (long)(*temperature_milli_c % 1000));

    return OPRT_OK;
}

M117B_STAGE_E m117b_get_last_error_stage(void)
{
    return s_last_error_stage;
}

const char *m117b_get_stage_name(M117B_STAGE_E stage)
{
    switch (stage) {
        case M117B_STAGE_RESET:
            return "reset";
        case M117B_STAGE_CONVERT:
            return "convert";
        case M117B_STAGE_READ:
            return "read";
        case M117B_STAGE_CRC:
            return "crc";
        default:
            return "none";
    }
}
