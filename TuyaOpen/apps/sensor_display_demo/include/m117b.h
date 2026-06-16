#ifndef __M117B_H__
#define __M117B_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    M117B_STAGE_NONE = 0,
    M117B_STAGE_RESET,
    M117B_STAGE_CONVERT,
    M117B_STAGE_READ,
    M117B_STAGE_CRC,
} M117B_STAGE_E;

OPERATE_RET m117b_init(void);
OPERATE_RET m117b_read_temperature(int32_t *temperature_milli_c);
M117B_STAGE_E m117b_get_last_error_stage(void);
const char *m117b_get_stage_name(M117B_STAGE_E stage);

#ifdef __cplusplus
}
#endif

#endif /* __M117B_H__ */
