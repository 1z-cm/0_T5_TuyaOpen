#ifndef __SENSOR_I2C_BUS_H__
#define __SENSOR_I2C_BUS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET sensor_i2c_bus_init(void);
OPERATE_RET sensor_i2c_bus_send(uint16_t address, const uint8_t *data, uint32_t size);
OPERATE_RET sensor_i2c_bus_receive(uint16_t address, uint8_t *data, uint32_t size);
OPERATE_RET sensor_i2c_m117b_send(uint16_t address, const uint8_t *data, uint32_t size);
OPERATE_RET sensor_i2c_m117b_receive(uint16_t address, uint8_t *data, uint32_t size);
OPERATE_RET sensor_i2c_bus_recover(void);
OPERATE_RET sensor_i2c_reg_write(uint16_t address, uint8_t reg, const uint8_t *data, uint32_t size);
OPERATE_RET sensor_i2c_reg_read(uint16_t address, uint8_t reg, uint8_t *data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_I2C_BUS_H__ */
