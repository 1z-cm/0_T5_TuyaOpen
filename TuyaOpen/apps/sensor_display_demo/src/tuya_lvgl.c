#include "tal_api.h"
#include "lv_vendor.h"
#include "tuya_lvgl.h"

OPERATE_RET tuya_lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);
    lv_vendor_start(5, 1024 * 8);

    return OPRT_OK;
}

OPERATE_RET tuya_lvgl_mutex_lock(void)
{
    lv_vendor_disp_lock();

    return OPRT_OK;
}

OPERATE_RET tuya_lvgl_mutex_unlock(void)
{
    lv_vendor_disp_unlock();

    return OPRT_OK;
}
