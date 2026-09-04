#include "Ams/bq796xx_protocol.h"

#include <stddef.h>
#include <string.h>

uint16_t bq796xx_crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t index;

    if (data == NULL) {
        return 0U;
    }
    for (index = 0U; index < length; index++) {
        uint8_t bit;
        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++) {
            crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0xA001U) :
                (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

uint8_t bq796xx_crc_valid(const uint8_t *frame, uint16_t length)
{
    if (frame == NULL || length < 3U) {
        return 0U;
    }
    return (bq796xx_crc16(frame, length) == 0U) ? 1U : 0U;
}

static uint16_t finish_frame(uint8_t *frame, uint16_t length)
{
    const uint16_t crc = bq796xx_crc16(frame, length);
    frame[length] = (uint8_t)crc;
    frame[length + 1U] = (uint8_t)(crc >> 8);
    return (uint16_t)(length + 2U);
}

uint16_t bq796xx_build_write(bq796xx_address_mode_t mode,
    uint8_t device_address, uint16_t register_address,
    const uint8_t *data, uint8_t data_length, uint8_t *frame,
    uint16_t capacity)
{
    uint16_t position = 0U;
    uint8_t index;
    uint8_t command;
    const uint16_t needed = (uint16_t)data_length +
        ((mode == BQ796XX_SINGLE) ? 6U : 5U);

    if (frame == NULL || data == NULL || data_length == 0U ||
        data_length > 8U || mode > BQ796XX_BROADCAST ||
        capacity < needed) {
        return 0U;
    }
    command = (mode == BQ796XX_SINGLE) ? 0x90U :
        ((mode == BQ796XX_STACK) ? 0xB0U : 0xD0U);
    frame[position++] = (uint8_t)(command | (data_length - 1U));
    if (mode == BQ796XX_SINGLE) {
        frame[position++] = device_address;
    }
    frame[position++] = (uint8_t)(register_address >> 8);
    frame[position++] = (uint8_t)register_address;
    for (index = 0U; index < data_length; index++) {
        frame[position++] = data[index];
    }
    return finish_frame(frame, position);
}

uint16_t bq796xx_build_read(bq796xx_address_mode_t mode,
    uint8_t device_address, uint16_t register_address,
    uint8_t response_length, uint8_t *frame, uint16_t capacity)
{
    uint16_t position = 0U;
    const uint16_t needed = (mode == BQ796XX_SINGLE) ? 7U : 6U;

    if (frame == NULL || response_length == 0U ||
        mode > BQ796XX_BROADCAST || capacity < needed) {
        return 0U;
    }
    frame[position++] = (mode == BQ796XX_SINGLE) ? 0x80U :
        ((mode == BQ796XX_STACK) ? 0xA0U : 0xC0U);
    if (mode == BQ796XX_SINGLE) {
        frame[position++] = device_address;
    }
    frame[position++] = (uint8_t)(register_address >> 8);
    frame[position++] = (uint8_t)register_address;
    frame[position++] = (uint8_t)(response_length - 1U);
    return finish_frame(frame, position);
}

bq796xx_response_status_t bq796xx_parse_response(
    const uint8_t *frame, uint16_t frame_length,
    uint8_t *device_address, uint16_t *register_address,
    uint8_t *data, uint8_t data_capacity)
{
    uint8_t data_length;
    uint16_t expected_length;

    if (frame == NULL || device_address == NULL ||
        register_address == NULL || data == NULL) {
        return BQ796XX_RESPONSE_INVALID_ARGUMENT;
    }
    if (frame_length < 7U) {
        return BQ796XX_RESPONSE_BAD_LENGTH;
    }
    /* A device response has a zero command bit and encodes N-1 data bytes
     * in INIT[6:0]. Command frames (INIT[7] set) are never valid here. */
    if ((frame[0] & 0x80U) != 0U) {
        return BQ796XX_RESPONSE_BAD_FORMAT;
    }
    data_length = (uint8_t)((frame[0] & 0x7FU) + 1U);
    expected_length = (uint16_t)data_length + 6U;
    if (frame_length != expected_length || data_capacity < data_length) {
        return BQ796XX_RESPONSE_BAD_LENGTH;
    }
    if (bq796xx_crc_valid(frame, frame_length) == 0U) {
        return BQ796XX_RESPONSE_BAD_CRC;
    }
    *device_address = frame[1];
    *register_address = (uint16_t)((uint16_t)frame[2] << 8) | frame[3];
    memcpy(data, &frame[4], data_length);
    return BQ796XX_RESPONSE_OK;
}
