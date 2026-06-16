#include "tal_api.h"

#include "sensor_i2c_bus.h"
#include "sh3001.h"

#define SH3001_ADDRESS_LOW  0x36
#define SH3001_ADDRESS_HIGH 0x37
#define SH3001_CHIP_ID_REG  0x0F
#define SH3001_CHIP_ID      0x61
#define SH3001_DATA_REG     0x00

static uint8_t s_address = 0;

static OPERATE_RET __read(uint8_t reg, uint8_t *data, uint32_t size)
{
    return sensor_i2c_reg_read(s_address, reg, data, size);
}

static OPERATE_RET __write(uint8_t reg, uint8_t value)
{
    return sensor_i2c_reg_write(s_address, reg, &value, 1);
}

static OPERATE_RET __update_bits(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t data;
    OPERATE_RET ret = __read(reg, &data, 1);
    if (ret != OPRT_OK) {
        return ret;
    }

    data = (data & mask) | value;
    return __write(reg, data);
}

static OPERATE_RET __detect(void)
{
    const uint8_t addresses[] = {SH3001_ADDRESS_HIGH, SH3001_ADDRESS_LOW};
    uint8_t chip_id = 0;

    for (uint32_t i = 0; i < sizeof(addresses); i++) {
        s_address = addresses[i];
        if (__read(SH3001_CHIP_ID_REG, &chip_id, 1) == OPRT_OK) {
            PR_NOTICE("SH3001 probe address 0x%02X: chip ID=0x%02X", s_address, chip_id);
            if (chip_id == SH3001_CHIP_ID) {
                return OPRT_OK;
            }
        }
    }

    s_address = 0;
    return OPRT_COM_ERROR;
}

static OPERATE_RET __soft_reset(void)
{
    OPERATE_RET ret;

    ret = __write(0xD4, 0x84);
    if (ret != OPRT_OK) return ret;
    tal_system_sleep(1);
    ret = __write(0xD4, 0x04);
    if (ret != OPRT_OK) return ret;
    tal_system_sleep(1);
    ret = __write(0x2F, 0x08);
    if (ret != OPRT_OK) return ret;
    tal_system_sleep(1);
    ret = __write(0x00, 0x73);
    if (ret != OPRT_OK) return ret;
    tal_system_sleep(50);

    return OPRT_OK;
}

static OPERATE_RET __drive_start(void)
{
    const uint8_t regs[] = {0x2E, 0xC0, 0xC1};
    const uint8_t start_values[] = {0xBF, 0x38, 0xFF};
    uint8_t saved[3];
    OPERATE_RET ret;

    for (uint32_t i = 0; i < sizeof(regs); i++) {
        ret = __read(regs[i], &saved[i], 1);
        if (ret != OPRT_OK) return ret;
    }
    for (uint32_t i = 0; i < sizeof(regs); i++) {
        ret = __write(regs[i], start_values[i]);
        if (ret != OPRT_OK) return ret;
    }
    tal_system_sleep(100);
    for (uint32_t i = 0; i < sizeof(regs); i++) {
        ret = __write(regs[i], saved[i]);
        if (ret != OPRT_OK) return ret;
    }
    tal_system_sleep(50);

    return OPRT_OK;
}

static OPERATE_RET __adc_reset(void)
{
    uint8_t d3;
    uint8_t d5;
    OPERATE_RET ret;

    ret = __read(0xD3, &d3, 1);
    if (ret != OPRT_OK) return ret;
    ret = __read(0xD5, &d5, 1);
    if (ret != OPRT_OK) return ret;

    d3 = (d3 & 0xFC) | 0x01;
    d5 = (d5 & 0xF9) | 0x02;
    ret = __write(0xD5, d5);
    if (ret != OPRT_OK) return ret;
    ret = __write(0xD3, d3);
    if (ret != OPRT_OK) return ret;
    tal_system_sleep(1);

    d3 = (d3 & 0xFC) | 0x02;
    ret = __write(0xD3, d3);
    if (ret != OPRT_OK) return ret;
    tal_system_sleep(1);

    d5 &= 0xF9;
    ret = __write(0xD5, d5);
    if (ret != OPRT_OK) return ret;
    tal_system_sleep(50);

    return OPRT_OK;
}

static OPERATE_RET __cva_reset(void)
{
    uint8_t data;
    OPERATE_RET ret = __read(0xD4, &data, 1);
    if (ret != OPRT_OK) return ret;

    ret = __write(0xD4, data | 0x08);
    if (ret != OPRT_OK) return ret;
    tal_system_sleep(10);

    return __write(0xD4, data & 0xF7);
}

static OPERATE_RET __configure(void)
{
    OPERATE_RET ret;
    uint8_t acc_range;
    uint8_t gyro_range_x;

    ret = __update_bits(0x22, 0xFF, 0x01);
    if (ret != OPRT_OK) return ret;
    ret = __write(0x23, 0x01);
    if (ret != OPRT_OK) return ret;
    ret = __write(0x25, 0x02);
    if (ret != OPRT_OK) return ret;
    ret = __update_bits(0x26, 0x17, 0x20);
    if (ret != OPRT_OK) return ret;

    ret = __update_bits(0x28, 0xFF, 0x01);
    if (ret != OPRT_OK) return ret;
    ret = __write(0x29, 0x01);
    if (ret != OPRT_OK) return ret;
    ret = __update_bits(0x2B, 0xE3, 0x00);
    if (ret != OPRT_OK) return ret;
    ret = __write(0x8F, 0x06);
    if (ret != OPRT_OK) return ret;
    ret = __write(0x9F, 0x06);
    if (ret != OPRT_OK) return ret;
    ret = __write(0xAF, 0x06);
    if (ret != OPRT_OK) return ret;

    ret = __read(0x25, &acc_range, 1);
    if (ret != OPRT_OK) return ret;
    ret = __read(0x8F, &gyro_range_x, 1);
    if (ret != OPRT_OK) return ret;

    PR_NOTICE("SH3001 range registers: ACC=0x%02X, GYRO=0x%02X",
              acc_range & 0x07, gyro_range_x & 0x07);
    if ((acc_range & 0x07) != 0x02 || (gyro_range_x & 0x07) != 0x06) {
        PR_ERR("SH3001 range register verification failed");
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

OPERATE_RET sh3001_init(void)
{
    OPERATE_RET ret = sensor_i2c_bus_init();
    if (ret != OPRT_OK) {
        return ret;
    }

    ret = __detect();
    if (ret != OPRT_OK) {
        PR_ERR("SH3001 not found at 0x36 or 0x37");
        return ret;
    }

    ret = __soft_reset();
    if (ret != OPRT_OK) {
        PR_ERR("SH3001 soft reset failed: %d", ret);
        return ret;
    }
    ret = __drive_start();
    if (ret != OPRT_OK) {
        PR_ERR("SH3001 drive start failed: %d", ret);
        return ret;
    }
    ret = __adc_reset();
    if (ret != OPRT_OK) {
        PR_ERR("SH3001 ADC reset failed: %d", ret);
        return ret;
    }
    ret = __cva_reset();
    if (ret != OPRT_OK) {
        PR_ERR("SH3001 CVA reset failed: %d", ret);
        return ret;
    }
    ret = __configure();
    if (ret != OPRT_OK) {
        PR_ERR("SH3001 configure failed: %d", ret);
        return ret;
    }

    PR_NOTICE("SH3001 initialized at address 0x%02X", s_address);
    return OPRT_OK;
}

OPERATE_RET sh3001_read_data(SH3001_DATA_T *data)
{
    uint8_t raw[12];
    OPERATE_RET ret;

    if (data == NULL || s_address == 0) {
        return OPRT_INVALID_PARM;
    }

    ret = __read(SH3001_DATA_REG, raw, sizeof(raw));
    if (ret != OPRT_OK) {
        return ret;
    }

    data->acc[0] = (int16_t)(((uint16_t)raw[1] << 8) | raw[0]);
    data->acc[1] = (int16_t)(((uint16_t)raw[3] << 8) | raw[2]);
    data->acc[2] = (int16_t)(((uint16_t)raw[5] << 8) | raw[4]);
    data->gyro[0] = (int16_t)(((uint16_t)raw[7] << 8) | raw[6]);
    data->gyro[1] = (int16_t)(((uint16_t)raw[9] << 8) | raw[8]);
    data->gyro[2] = (int16_t)(((uint16_t)raw[11] << 8) | raw[10]);

    return OPRT_OK;
}

uint8_t sh3001_get_address(void)
{
    return s_address;
}
