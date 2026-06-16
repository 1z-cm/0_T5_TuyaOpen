/**
 * @file example_led-pixels.c
 * @brief WS2812 LED pixel demo on T5AI board
 *
 * @note Drive modes (set WS2812_DRIVE_MODE):
 *       WS2812_DRIVE_SPI0 - P16, SDK tdd_ws2812 + tdl_pixel (recommended)
 *       WS2812_DRIVE_SPI2 - P24, QSPI0 1-wire SPI encoding (T5 Core onboard LED)
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "tal_system.h"
#include "tal_api.h"

#include "tkl_output.h"

#include "board_com_api.h"

#if (WS2812_DRIVE_MODE == WS2812_DRIVE_SPI2)
#include "example_ws2812_spi2.h"
#endif

/* ---------------------------------------------------------------------------
 * Drive configuration
 * --------------------------------------------------------------------------- */
#define WS2812_DRIVE_SPI0 0
#define WS2812_DRIVE_SPI2 1

#define WS2812_DRIVE_MODE WS2812_DRIVE_SPI2

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define LED_PIXELS_TOTAL_NUM 1
#define COLOR_BRIGHTNESS     128
#define COLOR_CYCLE_MS       500

#if (WS2812_DRIVE_MODE == WS2812_DRIVE_SPI0)
#include "tdl_pixel_dev_manage.h"
#include "tdl_pixel_color_manage.h"
#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
#include "tdd_pixel_ws2812.h"
#include "tdd_pixel_type.h"
#endif
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} WS2812_RGB_T;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
#if (WS2812_DRIVE_MODE == WS2812_DRIVE_SPI0)
static PIXEL_HANDLE_T sg_pixels_handle = NULL;
#endif

static const WS2812_RGB_T cCOLOR_ARR[] = {
    {COLOR_BRIGHTNESS, 0, 0},
    {0, COLOR_BRIGHTNESS, 0},
    {0, 0, COLOR_BRIGHTNESS},
};

#if (WS2812_DRIVE_MODE == WS2812_DRIVE_SPI0) && defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
/**
 * @brief Register WS2812 driver on SPI0 (P16 MOSI)
 * @return OPRT_OK on success
 */
static OPERATE_RET __ws2812_spi0_init(VOID_T)
{
    OPERATE_RET rt = OPRT_OK;
    PIXEL_DRIVER_CONFIG_T cfg = {
        .port = TUYA_SPI_NUM_0,
        .line_seq = GRB_ORDER,
    };

    TUYA_CALL_ERR_RETURN(tdd_ws2812_driver_register(LEDS_PIXEL_NAME, &cfg));
    TUYA_CALL_ERR_RETURN(tdl_pixel_dev_find(LEDS_PIXEL_NAME, &sg_pixels_handle));

    PIXEL_DEV_CONFIG_T pixels_cfg = {
        .pixel_num = LED_PIXELS_TOTAL_NUM,
        .pixel_resolution = 1000,
    };
    TUYA_CALL_ERR_RETURN(tdl_pixel_dev_open(sg_pixels_handle, &pixels_cfg));

    PR_NOTICE("WS2812 SPI0 ready, data pin P16, pixel count: %d", LED_PIXELS_TOTAL_NUM);
    return rt;
}
#endif

/**
 * @brief Cycle red / green / blue on WS2812 pixels
 * @return OPRT_OK on success
 */
static OPERATE_RET __ws2812_show_color_cycle(VOID_T)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t i = 0;

    for (i = 0; i < CNTSOF(cCOLOR_ARR); i++) {
#if (WS2812_DRIVE_MODE == WS2812_DRIVE_SPI2)
        TUYA_CALL_ERR_RETURN(example_ws2812_spi2_show(cCOLOR_ARR[i].r, cCOLOR_ARR[i].g, cCOLOR_ARR[i].b,
                                                     LED_PIXELS_TOTAL_NUM));
#else
#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
        PIXEL_COLOR_T color = {0};

        color.red = cCOLOR_ARR[i].r * 1000 / 255;
        color.green = cCOLOR_ARR[i].g * 1000 / 255;
        color.blue = cCOLOR_ARR[i].b * 1000 / 255;
        TUYA_CALL_ERR_RETURN(tdl_pixel_set_single_color_all(sg_pixels_handle, &color));
        TUYA_CALL_ERR_RETURN(tdl_pixel_dev_refresh(sg_pixels_handle));
#else
        return OPRT_COM_ERROR;
#endif
#endif
        tal_system_sleep(COLOR_CYCLE_MS);
    }

    return rt;
}

/**
 * @brief Application entry
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);

    TUYA_CALL_ERR_GOTO(board_register_hardware(), __EXIT);

#if (WS2812_DRIVE_MODE == WS2812_DRIVE_SPI2)
#if defined(ENABLE_QSPI) && (ENABLE_QSPI)
    TUYA_CALL_ERR_GOTO(example_ws2812_spi2_init(), __EXIT);
    PR_NOTICE("WS2812 SPI2 demo, data pin P24, pixel count: %d", LED_PIXELS_TOTAL_NUM);
#else
    PR_ERR("SPI2 mode requires CONFIG_ENABLE_QSPI=y");
    rt = OPRT_COM_ERROR;
    goto __EXIT;
#endif
#else
#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
    TUYA_CALL_ERR_GOTO(__ws2812_spi0_init(), __EXIT);
#else
    PR_ERR("SPI0 mode requires ENABLE_SPI and ENABLE_LEDS_PIXEL");
    rt = OPRT_COM_ERROR;
    goto __EXIT;
#endif
#endif

    while (1) {
        TUYA_CALL_ERR_LOG(__ws2812_show_color_cycle());
    }

__EXIT:
    if (OPRT_OK != rt) {
        PR_ERR("example leds-pixel failed: %d", rt);
    }
    return;
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief Application thread entry
 * @param[in] arg Thread argument
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    (void)arg;
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";

    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
