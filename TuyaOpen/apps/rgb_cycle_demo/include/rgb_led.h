#ifndef __RGB_LED_H__
#define __RGB_LED_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET rgb_led_init(void);
OPERATE_RET rgb_led_set_color(uint8_t red, uint8_t green, uint8_t blue);
OPERATE_RET rgb_led_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* __RGB_LED_H__ */
