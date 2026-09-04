#include <assert.h>
#include <string.h>

#include "Ams/bq796xx_protocol.h"

static void test_ti_reference_frames(void)
{
    uint8_t frame[16] = {0};
    const uint8_t data = 0U;
    const uint8_t expected_write[] =
        {0xB0U, 0x03U, 0x43U, 0x00U, 0xE7U, 0xD4U};
    const uint8_t expected_read[] =
        {0xA0U, 0x03U, 0x43U, 0x00U, 0xE3U, 0x14U};
    uint16_t length;

    length = bq796xx_build_write(BQ796XX_STACK, 0U, 0x0343U,
        &data, 1U, frame, sizeof(frame));
    assert(length == sizeof(expected_write));
    assert(memcmp(frame, expected_write, sizeof(expected_write)) == 0);
    assert(bq796xx_crc_valid(frame, length) != 0U);

    length = bq796xx_build_read(BQ796XX_STACK, 0U, 0x0343U,
        1U, frame, sizeof(frame));
    assert(length == sizeof(expected_read));
    assert(memcmp(frame, expected_read, sizeof(expected_read)) == 0);
    assert(bq796xx_crc_valid(frame, length) != 0U);
}

static void test_response_parser(void)
{
    uint8_t frame[7] = {0x00U, 0x03U, 0x03U, 0x06U, 0x03U, 0U, 0U};
    uint8_t device = 0U;
    uint16_t address = 0U;
    uint8_t value = 0U;
    uint16_t crc = bq796xx_crc16(frame, 5U);

    frame[5] = (uint8_t)crc;
    frame[6] = (uint8_t)(crc >> 8);
    assert(bq796xx_parse_response(frame, sizeof(frame), &device, &address,
        &value, 1U) == BQ796XX_RESPONSE_OK);
    assert(device == 3U);
    assert(address == 0x0306U);
    assert(value == 3U);

    frame[6] ^= 0x01U;
    assert(bq796xx_parse_response(frame, sizeof(frame), &device, &address,
        &value, 1U) == BQ796XX_RESPONSE_BAD_CRC);
}

int main(void)
{
    test_ti_reference_frames();
    test_response_parser();
    return 0;
}
