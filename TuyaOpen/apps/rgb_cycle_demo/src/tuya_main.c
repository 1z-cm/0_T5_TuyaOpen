#include "tal_api.h"
#include "tkl_output.h"

#include "rgb_led.h"

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

#define RGB_CYCLE_INTERVAL_MS 1000
#define RGB_CYCLE_BRIGHTNESS   64

typedef struct {
    const char *name;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RGB_CYCLE_COLOR_T;

static const RGB_CYCLE_COLOR_T s_colors[] = {
    {.name = "RED",   .red = RGB_CYCLE_BRIGHTNESS, .green = 0,                    .blue = 0},
    {.name = "GREEN", .red = 0,                    .green = RGB_CYCLE_BRIGHTNESS, .blue = 0},
    {.name = "BLUE",  .red = 0,                    .green = 0,                    .blue = RGB_CYCLE_BRIGHTNESS},
};

static THREAD_HANDLE s_rgb_thread = NULL;

static void __rgb_cycle_thread(void *arg)
{
    OPERATE_RET ret;
    uint32_t cycle_count = 0;

    (void)arg;

    PR_NOTICE("RGB stage 1: WS2812 P25 GPIO timing initialization");
    ret = rgb_led_init();
    if (ret != OPRT_OK) {
        PR_ERR("RGB LED init failed: %d", ret);
        goto exit;
    }

    PR_NOTICE("RGB stage 2: start color cycle");
    while (1) {
        for (uint32_t i = 0; i < CNTSOF(s_colors); i++) {
            ret = rgb_led_set_color(s_colors[i].red, s_colors[i].green, s_colors[i].blue);
            if (ret != OPRT_OK) {
                PR_ERR("set RGB color failed: %d", ret);
                goto exit;
            }

            PR_NOTICE("RGB cycle %lu: %s", (unsigned long)cycle_count, s_colors[i].name);
            tal_system_sleep(RGB_CYCLE_INTERVAL_MS);
        }
        cycle_count++;
    }

exit:
    rgb_led_clear();
    PR_ERR("RGB cycle stopped; P25 held low");
    while (1) {
        tal_system_sleep(1000);
    }
}

void user_main(void)
{
    OPERATE_RET ret;
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 2048,
        .priority = THREAD_PRIO_2,
        .thrdname = "rgb_cycle",
    };

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);

    ret = tal_thread_create_and_start(&s_rgb_thread, NULL, NULL, __rgb_cycle_thread, NULL, &thread_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("create RGB cycle thread failed: %d", ret);
    }
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

static void __app_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(s_app_thread);
    s_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 4096,
        .priority = THREAD_PRIO_1,
        .thrdname = "tuya_app_main",
    };

    tal_thread_create_and_start(&s_app_thread, NULL, NULL, __app_thread, NULL, &thread_cfg);
}
#endif
