#include <string.h>

#include "tal_api.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"

#include "sensor_i2c_bus.h"

#define SENSOR_I2C_TRANSFER_MAX 32

/*
 * P42/P43 are an alternate hardware-I2C1 pin group on BK7258. The display
 * board also assigns I2C1 to touch, so this standalone sensor bus uses TKL's
 * software-I2C path on logical port 2 to keep both pin groups independent.
 */
#define SENSOR_I2C_PORT    TUYA_I2C_NUM_2
#define SENSOR_I2C_SCL_PIN TUYA_GPIO_NUM_42
#define SENSOR_I2C_SDA_PIN TUYA_GPIO_NUM_43

#define SENSOR_I2C_HALF_PERIOD_US      5
#define SENSOR_M117B_START_HOLD_US    60
#define SENSOR_I2C_RECOVERY_CLOCKS     9

extern void bk_delay_us(uint32_t us);

static bool s_bus_initialized = false;

static void __sensor_i2c_delay(uint32_t us)
{
    bk_delay_us(us);
}

static OPERATE_RET __sensor_i2c_sda_output(TUYA_GPIO_LEVEL_E level)
{
    TUYA_GPIO_BASE_CFG_T cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = level,
    };

    return tkl_gpio_init(SENSOR_I2C_SDA_PIN, &cfg);
}

static OPERATE_RET __sensor_i2c_sda_input(void)
{
    TUYA_GPIO_BASE_CFG_T cfg = {
        .mode = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
        .level = TUYA_GPIO_LEVEL_HIGH,
    };

    return tkl_gpio_init(SENSOR_I2C_SDA_PIN, &cfg);
}

static OPERATE_RET __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_E level)
{
    return tkl_gpio_write(SENSOR_I2C_SCL_PIN, level);
}

static OPERATE_RET __sensor_i2c_sda_write(TUYA_GPIO_LEVEL_E level)
{
    return tkl_gpio_write(SENSOR_I2C_SDA_PIN, level);
}

static OPERATE_RET __sensor_i2c_m117b_start(void)
{
    OPERATE_RET ret;

    ret = __sensor_i2c_sda_output(TUYA_GPIO_LEVEL_HIGH);
    if (ret != OPRT_OK) {
        return ret;
    }

    ret = __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_HIGH);
    if (ret != OPRT_OK) {
        return ret;
    }
    __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);

    ret = __sensor_i2c_sda_write(TUYA_GPIO_LEVEL_LOW);
    if (ret != OPRT_OK) {
        return ret;
    }

    /* M117B requires at least 50 us START hold time near 3.3 V. */
    __sensor_i2c_delay(SENSOR_M117B_START_HOLD_US);
    return __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_LOW);
}

static OPERATE_RET __sensor_i2c_stop(void)
{
    OPERATE_RET ret;

    ret = __sensor_i2c_sda_output(TUYA_GPIO_LEVEL_LOW);
    if (ret != OPRT_OK) {
        return ret;
    }

    ret = __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_HIGH);
    if (ret != OPRT_OK) {
        return ret;
    }
    __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);

    ret = __sensor_i2c_sda_write(TUYA_GPIO_LEVEL_HIGH);
    __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);
    return ret;
}

static OPERATE_RET __sensor_i2c_write_byte(uint8_t value)
{
    OPERATE_RET ret;
    TUYA_GPIO_LEVEL_E ack_level = TUYA_GPIO_LEVEL_HIGH;

    ret = __sensor_i2c_sda_output(TUYA_GPIO_LEVEL_HIGH);
    if (ret != OPRT_OK) {
        return ret;
    }

    for (uint8_t bit = 0; bit < 8; bit++) {
        ret = __sensor_i2c_sda_write((value & 0x80) ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW);
        if (ret != OPRT_OK) {
            return ret;
        }
        value <<= 1;
        __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);

        ret = __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_HIGH);
        if (ret != OPRT_OK) {
            return ret;
        }
        __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);
        ret = __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_LOW);
        if (ret != OPRT_OK) {
            return ret;
        }
    }

    ret = __sensor_i2c_sda_input();
    if (ret != OPRT_OK) {
        return ret;
    }
    __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);

    ret = __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_HIGH);
    if (ret != OPRT_OK) {
        return ret;
    }
    __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);
    ret = tkl_gpio_read(SENSOR_I2C_SDA_PIN, &ack_level);
    __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_LOW);
    if (ret != OPRT_OK) {
        return ret;
    }

    return (ack_level == TUYA_GPIO_LEVEL_LOW) ? OPRT_OK : OPRT_COM_ERROR;
}

static OPERATE_RET __sensor_i2c_read_byte(uint8_t *value, bool acknowledge)
{
    OPERATE_RET ret;
    TUYA_GPIO_LEVEL_E bit_level;
    uint8_t data = 0;

    ret = __sensor_i2c_sda_input();
    if (ret != OPRT_OK) {
        return ret;
    }

    for (uint8_t bit = 0; bit < 8; bit++) {
        __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);
        ret = __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_HIGH);
        if (ret != OPRT_OK) {
            return ret;
        }
        __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);

        ret = tkl_gpio_read(SENSOR_I2C_SDA_PIN, &bit_level);
        if (ret != OPRT_OK) {
            __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_LOW);
            return ret;
        }
        data = (uint8_t)((data << 1) | (bit_level == TUYA_GPIO_LEVEL_HIGH ? 1 : 0));

        ret = __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_LOW);
        if (ret != OPRT_OK) {
            return ret;
        }
    }

    ret = __sensor_i2c_sda_output(acknowledge ? TUYA_GPIO_LEVEL_LOW : TUYA_GPIO_LEVEL_HIGH);
    if (ret != OPRT_OK) {
        return ret;
    }
    __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);
    ret = __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_HIGH);
    if (ret != OPRT_OK) {
        return ret;
    }
    __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);
    ret = __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_LOW);
    *value = data;
    return ret;
}

OPERATE_RET sensor_i2c_bus_init(void)
{
    OPERATE_RET ret;
    TUYA_IIC_BASE_CFG_T cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .speed = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
    };

    if (s_bus_initialized) {
        return OPRT_OK;
    }

    ret = tkl_io_pinmux_config(SENSOR_I2C_SCL_PIN, TUYA_IIC2_SCL);
    if (ret != OPRT_OK) {
        PR_ERR("sensor I2C SCL pinmux failed: %d", ret);
        return ret;
    }

    ret = tkl_io_pinmux_config(SENSOR_I2C_SDA_PIN, TUYA_IIC2_SDA);
    if (ret != OPRT_OK) {
        PR_ERR("sensor I2C SDA pinmux failed: %d", ret);
        return ret;
    }

    ret = tkl_i2c_init(SENSOR_I2C_PORT, &cfg);
    if (ret != OPRT_OK) {
        PR_ERR("sensor I2C init failed: %d", ret);
        return ret;
    }

    s_bus_initialized = true;
    PR_NOTICE("sensor software I2C ready: port=%d, SCL=P42, SDA=P43", SENSOR_I2C_PORT);

    return OPRT_OK;
}

OPERATE_RET sensor_i2c_bus_send(uint16_t address, const uint8_t *data, uint32_t size)
{
    if (!s_bus_initialized || data == NULL || size == 0) {
        return OPRT_INVALID_PARM;
    }

    return tkl_i2c_master_send(SENSOR_I2C_PORT, address, data, size, FALSE);
}

OPERATE_RET sensor_i2c_bus_receive(uint16_t address, uint8_t *data, uint32_t size)
{
    if (!s_bus_initialized || data == NULL || size == 0) {
        return OPRT_INVALID_PARM;
    }

    return tkl_i2c_master_receive(SENSOR_I2C_PORT, address, data, size, FALSE);
}

OPERATE_RET sensor_i2c_m117b_send(uint16_t address, const uint8_t *data, uint32_t size)
{
    OPERATE_RET ret;

    if (!s_bus_initialized || data == NULL || size == 0 || address > 0x7F) {
        return OPRT_INVALID_PARM;
    }

    ret = __sensor_i2c_m117b_start();
    if (ret == OPRT_OK) {
        ret = __sensor_i2c_write_byte((uint8_t)(address << 1));
    }
    for (uint32_t i = 0; ret == OPRT_OK && i < size; i++) {
        ret = __sensor_i2c_write_byte(data[i]);
    }

    __sensor_i2c_stop();
    return ret;
}

OPERATE_RET sensor_i2c_m117b_receive(uint16_t address, uint8_t *data, uint32_t size)
{
    OPERATE_RET ret;

    if (!s_bus_initialized || data == NULL || size == 0 || address > 0x7F) {
        return OPRT_INVALID_PARM;
    }

    ret = __sensor_i2c_m117b_start();
    if (ret == OPRT_OK) {
        ret = __sensor_i2c_write_byte((uint8_t)((address << 1) | 1));
    }
    for (uint32_t i = 0; ret == OPRT_OK && i < size; i++) {
        ret = __sensor_i2c_read_byte(&data[i], (i + 1) < size);
    }

    __sensor_i2c_stop();
    return ret;
}

OPERATE_RET sensor_i2c_bus_recover(void)
{
    OPERATE_RET ret;

    if (!s_bus_initialized) {
        return OPRT_INVALID_PARM;
    }

    ret = __sensor_i2c_sda_input();
    if (ret != OPRT_OK) {
        return ret;
    }

    for (uint8_t pulse = 0; pulse < SENSOR_I2C_RECOVERY_CLOCKS; pulse++) {
        ret = __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_LOW);
        if (ret != OPRT_OK) {
            return ret;
        }
        __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);
        ret = __sensor_i2c_scl_write(TUYA_GPIO_LEVEL_HIGH);
        if (ret != OPRT_OK) {
            return ret;
        }
        __sensor_i2c_delay(SENSOR_I2C_HALF_PERIOD_US);
    }

    ret = __sensor_i2c_stop();
    if (ret == OPRT_OK) {
        PR_DEBUG("sensor I2C recovered with %u SCL pulses", SENSOR_I2C_RECOVERY_CLOCKS);
    }
    return ret;
}

OPERATE_RET sensor_i2c_reg_write(uint16_t address, uint8_t reg, const uint8_t *data, uint32_t size)
{
    uint8_t buffer[SENSOR_I2C_TRANSFER_MAX + 1];

    if (!s_bus_initialized || data == NULL || size == 0 || size > SENSOR_I2C_TRANSFER_MAX) {
        return OPRT_INVALID_PARM;
    }

    buffer[0] = reg;
    memcpy(&buffer[1], data, size);

    return tkl_i2c_master_send(SENSOR_I2C_PORT, address, buffer, size + 1, FALSE);
}

OPERATE_RET sensor_i2c_reg_read(uint16_t address, uint8_t reg, uint8_t *data, uint32_t size)
{
    OPERATE_RET ret;

    if (!s_bus_initialized || data == NULL || size == 0) {
        return OPRT_INVALID_PARM;
    }

    ret = tkl_i2c_master_send(SENSOR_I2C_PORT, address, &reg, 1, TRUE);
    if (ret != OPRT_OK) {
        return ret;
    }

    return tkl_i2c_master_receive(SENSOR_I2C_PORT, address, data, size, FALSE);
}
