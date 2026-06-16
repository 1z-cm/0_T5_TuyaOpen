/**
 * @file example_ws2812_spi2.h
 * @brief WS2812 driver on T5 SPI2 (QSPI0 IO0 / P24)
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#ifndef __EXAMPLE_WS2812_SPI2_H__
#define __EXAMPLE_WS2812_SPI2_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize QSPI0 (TUYA_SPI_NUM_2) for WS2812 on P24
 * @return OPRT_OK on success
 */
OPERATE_RET example_ws2812_spi2_init(VOID_T);

/**
 * @brief Deinitialize QSPI0 WS2812 driver
 * @return OPRT_OK on success
 */
OPERATE_RET example_ws2812_spi2_deinit(VOID_T);

/**
 * @brief Set all pixels to the same RGB color (GRB wire order)
 * @param[in] r Red level 0-255
 * @param[in] g Green level 0-255
 * @param[in] b Blue level 0-255
 * @param[in] pixel_num Number of chained WS2812 LEDs
 * @return OPRT_OK on success
 */
OPERATE_RET example_ws2812_spi2_show(uint8_t r, uint8_t g, uint8_t b, uint16_t pixel_num);

#ifdef __cplusplus
}
#endif

#endif /* __EXAMPLE_WS2812_SPI2_H__ */
