#ifndef BQ796XX_PROTOCOL_H
#define BQ796XX_PROTOCOL_H

#include <stdint.h>

typedef enum {
    BQ796XX_SINGLE = 0,
    BQ796XX_STACK,
    BQ796XX_BROADCAST
} bq796xx_address_mode_t;

typedef enum {
    BQ796XX_RESPONSE_OK = 0,
    BQ796XX_RESPONSE_INVALID_ARGUMENT,
    BQ796XX_RESPONSE_BAD_LENGTH,
    BQ796XX_RESPONSE_BAD_FORMAT,
    BQ796XX_RESPONSE_BAD_CRC
} bq796xx_response_status_t;

uint16_t bq796xx_crc16(const uint8_t *data, uint16_t length);
uint8_t bq796xx_crc_valid(const uint8_t *frame, uint16_t length);
uint16_t bq796xx_build_write(bq796xx_address_mode_t mode,
    uint8_t device_address, uint16_t register_address,
    const uint8_t *data, uint8_t data_length, uint8_t *frame,
    uint16_t capacity);
uint16_t bq796xx_build_read(bq796xx_address_mode_t mode,
    uint8_t device_address, uint16_t register_address,
    uint8_t response_length, uint8_t *frame, uint16_t capacity);
bq796xx_response_status_t bq796xx_parse_response(
    const uint8_t *frame, uint16_t frame_length,
    uint8_t *device_address, uint16_t *register_address,
    uint8_t *data, uint8_t data_capacity);

#endif /* BQ796XX_PROTOCOL_H */
