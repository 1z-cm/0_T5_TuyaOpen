/**
 * @file example_ws2812_spi2.c
 * @brief WS2812 on T5 SPI2 (QSPI0 1-wire, data pin P24)
 *
 * TUYA_SPI_NUM_2 maps to QSPI0 IO0 (GPIO_24). Uses 0xC0/0xF0 SPI encoding via
 * tkl_qspi_comand() 1-wire stream. LCD_CMD path runs ~3x slower than cfg freq,
 * so WS2812_SPI_SPEED_HZ applies 3x compensation (19.5MHz cfg -> ~6.5MHz effective).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "example_ws2812_spi2.h"

#include <string.h>

#include "tal_api.h"
#include "tkl_qspi.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define WS2812_QSPI_PORT                 TUYA_QSPI_NUM_0
#define WS2812_SPI_TARGET_HZ             6500000U
#define WS2812_QSPI_LCD_CLK_COMPENSATION 3U
#define WS2812_SPI_SPEED_HZ              (WS2812_SPI_TARGET_HZ * WS2812_QSPI_LCD_CLK_COMPENSATION)
#define WS2812_SPI_CODE_0                0xC0
#define WS2812_SPI_CODE_1                0xF0
#define WS2812_ONE_BYTE_LEN              8
#define WS2812_QSPI_CHUNK_SIZE           256
#define WS2812_QSPI_TX_SIZE              512

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static uint8_t s_tx_buf[WS2812_QSPI_TX_SIZE];
static uint8_t s_inited = 0;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Convert one RGB byte to WS2812 SPI bitstream
 * @param[in] color_data Color byte
 * @param[out] spi_data_buf Output buffer, at least 8 bytes
 * @return none
 */
static void __ws2812_rgb_to_spi(uint8_t color_data, uint8_t *spi_data_buf)
{
    uint8_t i = 0;

    for (i = 0; i < WS2812_ONE_BYTE_LEN; i++) {
        spi_data_buf[i] = (color_data & 0x80) ? WS2812_SPI_CODE_1 : WS2812_SPI_CODE_0;
        color_data <<= 1;
    }
}

/**
 * @brief Stream one chunk on QSPI0 IO0 without command/address prefix
 * @param[in] data Data buffer
 * @param[in] len Chunk length, 1..256
 * @return OPRT_OK on success
 */
static OPERATE_RET __ws2812_qspi_stream_send(uint8_t *data, uint32_t len)
{
    TUYA_QSPI_CMD_T cmd = {0};

    if ((data == NULL) || (len == 0) || (len > WS2812_QSPI_CHUNK_SIZE)) {
        return OPRT_INVALID_PARM;
    }

    cmd.op = TUYA_QSPI_WRITE;
    cmd.cmd_size = 0;
    cmd.addr_size = 0;
    cmd.cmd_lines = TUYA_QSPI_1WIRE;
    cmd.addr_lines = TUYA_QSPI_1WIRE;
    cmd.data = data;
    cmd.data_size = len;
    cmd.data_lines = TUYA_QSPI_1WIRE;
    cmd.dummy_cycle = 0;

    return tkl_qspi_comand(WS2812_QSPI_PORT, &cmd);
}

/**
 * @brief Send full TX buffer in 256-byte QSPI chunks
 * @param[in] tx_len Total length to send
 * @return OPRT_OK on success
 */
static OPERATE_RET __ws2812_qspi_send_buf(uint32_t tx_len)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t offset = 0;
    uint32_t chunk = 0;

    while (offset < tx_len) {
        chunk = tx_len - offset;
        if (chunk > WS2812_QSPI_CHUNK_SIZE) {
            chunk = WS2812_QSPI_CHUNK_SIZE;
        }
        TUYA_CALL_ERR_RETURN(__ws2812_qspi_stream_send(&s_tx_buf[offset], chunk));
        offset += chunk;
    }

    return rt;
}

/**
 * @brief Build WS2812 SPI TX buffer and return total length (GRB order)
 * @param[in] r Red level
 * @param[in] g Green level
 * @param[in] b Blue level
 * @param[in] pixel_num Pixel count
 * @param[out] tx_len Total stream length including reset padding
 * @return OPRT_OK on success
 */
static OPERATE_RET __ws2812_build_tx_buf(uint8_t r, uint8_t g, uint8_t b, uint16_t pixel_num, uint32_t *tx_len)
{
    uint32_t idx = 0;
    uint32_t payload_len = 0;
    uint16_t i = 0;

    if ((pixel_num == 0) || (tx_len == NULL)) {
        return OPRT_INVALID_PARM;
    }

    payload_len = (uint32_t)pixel_num * WS2812_ONE_BYTE_LEN * 3U;
    if (payload_len > WS2812_QSPI_TX_SIZE) {
        return OPRT_INVALID_PARM;
    }

    memset(s_tx_buf, 0x00, sizeof(s_tx_buf));

    for (i = 0; i < pixel_num; i++) {
        __ws2812_rgb_to_spi(g, &s_tx_buf[idx]);
        idx += WS2812_ONE_BYTE_LEN;
        __ws2812_rgb_to_spi(r, &s_tx_buf[idx]);
        idx += WS2812_ONE_BYTE_LEN;
        __ws2812_rgb_to_spi(b, &s_tx_buf[idx]);
        idx += WS2812_ONE_BYTE_LEN;
    }

    if (payload_len < WS2812_QSPI_CHUNK_SIZE) {
        *tx_len = WS2812_QSPI_CHUNK_SIZE;
    } else {
        *tx_len = payload_len;
    }

    return OPRT_OK;
}

/**
 * @brief Initialize QSPI0 (TUYA_SPI_NUM_2) for WS2812 on P24
 * @return OPRT_OK on success
 */
OPERATE_RET example_ws2812_spi2_init(VOID_T)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_QSPI_BASE_CFG_T qspi_cfg = {
        .role = TUYA_QSPI_ROLE_MASTER,
        .mode = TUYA_QSPI_MODE0,
        .type = TUYA_QSPI_TYPE_LCD,
        .freq_hz = WS2812_SPI_SPEED_HZ,
        .use_dma = FALSE,
        .dma_data_lines = TUYA_QSPI_1WIRE,
    };

    if (s_inited) {
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(tkl_qspi_init(WS2812_QSPI_PORT, &qspi_cfg));
    TUYA_CALL_ERR_RETURN(tkl_qspi_force_cs_pin(WS2812_QSPI_PORT, TUYA_GPIO_LEVEL_LOW));

    s_inited = 1;
    PR_NOTICE("WS2812 SPI2 ready, P24 (QSPI0 IO0), cfg=%u Hz", WS2812_SPI_SPEED_HZ);

    return rt;
}

/**
 * @brief Deinitialize QSPI0 WS2812 driver
 * @return OPRT_OK on success
 */
OPERATE_RET example_ws2812_spi2_deinit(VOID_T)
{
    if (!s_inited) {
        return OPRT_OK;
    }

    tkl_qspi_force_cs_pin(WS2812_QSPI_PORT, TUYA_GPIO_LEVEL_HIGH);
    tkl_qspi_deinit(WS2812_QSPI_PORT);
    s_inited = 0;

    return OPRT_OK;
}

/**
 * @brief Set all pixels to the same RGB color (GRB wire order)
 * @param[in] r Red level 0-255
 * @param[in] g Green level 0-255
 * @param[in] b Blue level 0-255
 * @param[in] pixel_num Number of chained WS2812 LEDs
 * @return OPRT_OK on success
 */
OPERATE_RET example_ws2812_spi2_show(uint8_t r, uint8_t g, uint8_t b, uint16_t pixel_num)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t tx_len = 0;

    if (!s_inited) {
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(__ws2812_build_tx_buf(r, g, b, pixel_num, &tx_len));
    TUYA_CALL_ERR_RETURN(__ws2812_qspi_send_buf(tx_len));

    return rt;
}
