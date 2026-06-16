#ifndef __SH3001_DEMO_H__
#define __SH3001_DEMO_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t acc[3];
    int16_t gyro[3];
} SH3001_DATA_T;

OPERATE_RET sh3001_init(void);
OPERATE_RET sh3001_read_data(SH3001_DATA_T *data);
uint8_t sh3001_get_address(void);

#ifdef __cplusplus
}
#endif

#endif /* __SH3001_DEMO_H__ */
