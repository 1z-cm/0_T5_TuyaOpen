#include "tal_api.h"
#include "tkl_gpio.h"

#include "rgb_led.h"

#define RGB_LED_DATA_PIN TUYA_GPIO_NUM_25
#define RGB_LED_COUNT    1U
#define RGB_LED_REPEATS  1U

#define RGB_LED_GPIO_HIGH     0x2U
#define RGB_LED_GPIO_LOW      0x0U
#define RGB_LED_AON_GPIO_BASE 0x44000400UL

/*
 * T5 application core runs at 120 MHz. The whole 24-bit frame is sent with
 * interrupts disabled so the WS2812 pulse widths cannot be stretched.
 */
#define RGB_LED_DELAY_T0H() __asm volatile(".rept 42\n nop\n .endr" ::: "memory")
#define RGB_LED_DELAY_T0L() __asm volatile(".rept 108\n nop\n .endr" ::: "memory")
#define RGB_LED_DELAY_T1H() __asm volatile(".rept 84\n nop\n .endr" ::: "memory")
#define RGB_LED_DELAY_T1L() __asm volatile(".rept 66\n nop\n .endr" ::: "memory")

static volatile uint32_t *const s_rgb_gpio_reg =
    (volatile uint32_t *)(RGB_LED_AON_GPIO_BASE + (RGB_LED_DATA_PIN * 4U));
static bool s_rgb_led_initialized = false;

static inline __attribute__((always_inline)) void __rgb_led_send_bit(bool one)
{
    *s_rgb_gpio_reg = RGB_LED_GPIO_HIGH;
    if (one) {
        RGB_LED_DELAY_T1H();
        *s_rgb_gpio_reg = RGB_LED_GPIO_LOW;
        RGB_LED_DELAY_T1L();
    } else {
        RGB_LED_DELAY_T0H();
        *s_rgb_gpio_reg = RGB_LED_GPIO_LOW;
        RGB_LED_DELAY_T0L();
    }
}

static inline __attribute__((always_inline)) void __rgb_led_send_byte(uint8_t value)
{
    for (uint8_t bit = 0; bit < 8U; bit++) {
        __rgb_led_send_bit((value & 0x80U) != 0U);
        value <<= 1;
    }
}

static __attribute__((section(".itcm_sec_code"), noinline))
void __rgb_led_send_frame(uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t irq_level = tal_system_enter_critical();

    for (uint32_t led = 0; led < RGB_LED_COUNT; led++) {
        __rgb_led_send_byte(green);
        __rgb_led_send_byte(red);
        __rgb_led_send_byte(blue);
    }

    *s_rgb_gpio_reg = RGB_LED_GPIO_LOW;
    tal_system_exit_critical(irq_level);

    tal_system_sleep(1);
}

OPERATE_RET rgb_led_init(void)
{
    OPERATE_RET ret;
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_LOW,
    };

    if (s_rgb_led_initialized) {
        return OPRT_OK;
    }

    ret = tkl_gpio_init(RGB_LED_DATA_PIN, &gpio_cfg);
    if (ret != OPRT_OK) {
        return ret;
    }

    s_rgb_led_initialized = true;
    rgb_led_clear();

    PR_NOTICE("WS2812 ready: P25 GPIO, GRB, count=%u, repeat=%u",
              RGB_LED_COUNT, RGB_LED_REPEATS);
    return OPRT_OK;
}

OPERATE_RET rgb_led_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!s_rgb_led_initialized) {
        return OPRT_COM_ERROR;
    }

    for (uint32_t repeat = 0; repeat < RGB_LED_REPEATS; repeat++) {
        __rgb_led_send_frame(red, green, blue);
    }

    return OPRT_OK;
}

OPERATE_RET rgb_led_clear(void)
{
    return rgb_led_set_color(0, 0, 0);
}
