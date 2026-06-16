#include "tal_api.h"
#include "tal_cli.h"
#include "tkl_output.h"
#include "tuya_cloud_types.h"

#include "board_com_api.h"
#include "sensor_service.h"
#include "sensor_ui.h"
#include "tuya_lvgl.h"

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.5.0"
#endif

void user_main(void)
{
    OPERATE_RET ret = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    tal_sw_timer_init();
    tal_workq_init();
    tal_time_service_init();
    tal_cli_init();

    ret = board_register_hardware();
    if (ret != OPRT_OK) {
        PR_ERR("board_register_hardware failed: %d", ret);
        return;
    }

    ret = tuya_lvgl_init();
    if (ret != OPRT_OK) {
        PR_ERR("tuya_lvgl_init failed: %d", ret);
        return;
    }

    ret = sensor_ui_init();
    if (ret != OPRT_OK) {
        PR_ERR("sensor_ui_init failed: %d", ret);
        return;
    }

    ret = sensor_service_start();
    if (ret != OPRT_OK) {
        PR_ERR("sensor_service_start failed: %d", ret);
        return;
    }

    PR_NOTICE("temperature and SH3001 display demo started");
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    user_main();
}
#else
static THREAD_HANDLE s_app_thread = NULL;

static void tuya_app_thread(void *arg)
{
    (void)arg;

    user_main();
    tal_thread_delete(s_app_thread);
    s_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {
        .stackDepth = 1024 * 8,
        .priority = THREAD_PRIO_4,
        .thrdname = "sensor_display",
    };

    tal_thread_create_and_start(&s_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
