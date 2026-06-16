#ifndef __TUYA_LVGL_H__
#define __TUYA_LVGL_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET tuya_lvgl_init(void);
OPERATE_RET tuya_lvgl_mutex_lock(void);
OPERATE_RET tuya_lvgl_mutex_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_LVGL_H__ */
