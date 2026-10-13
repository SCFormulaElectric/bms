#include "Ams/bq79616_measurement.h"

#include <stddef.h>
#include <string.h>

#include "Ams/bq796xx_protocol.h"

#define BQ79616_MAX_DEVICES          63U
#define BQ79616_CELL_UV_X100      19073
#define BQ79616_GPIO_UV_X100      15259

uint8_t bq79616_active_cell_value(uint8_t cells)
{
    if (cells < BQ79616_MIN_CELLS || cells > BQ79616_MAX_CELLS) {
        return 0U;
    }
    return (uint8_t)(cells - BQ79616_MIN_CELLS);
}

uint16_t bq79616_cell_block_address(uint8_t cells)
{
    if (cells < BQ79616_MIN_CELLS || cells > BQ79616_MAX_CELLS) {
        cells = BQ79616_MAX_CELLS;
    }
    return (uint16_t)(BQ79616_REG_VCELL16_HI +
        (uint16_t)(BQ79616_MAX_CELLS - cells) * 2U);
}

uint8_t bq79616_gpio_voltage_conf(uint8_t gpio_count,
    uint8_t conf[BQ79616_GPIO_CONF_REGISTERS])
{
    uint8_t gpio;

    if (conf == NULL || gpio_count > BQ79616_GPIO_COUNT) {
        return 0U;
    }
    memset(conf, 0, BQ79616_GPIO_CONF_REGISTERS);
    for (gpio = 0U; gpio < gpio_count; gpio++) {
        conf[gpio / 2U] |= (uint8_t)(BQ79616_GPIO_MODE_ADC_ONLY <<
            ((gpio % 2U) * 3U));
    }
    return 1U;
}

uint16_t bq79616_stack_response_bytes(uint8_t devices, uint8_t channels)
{
    return (uint16_t)((uint16_t)devices *
        (BQ79616_RESPONSE_OVERHEAD_BYTES + (uint16_t)channels * 2U));
}

static int32_t code_to_mv(int16_t code, int32_t uv_x100)
{
    const int32_t scaled = (int32_t)code * uv_x100;

    /* Round half away from zero; 100000 = uV*100 per mV. */
    return (scaled >= 0) ? (scaled + 50000) / 100000 :
        (scaled - 50000) / 100000;
}

int32_t bq79616_cell_code_to_mv(int16_t code)
{
    return code_to_mv(code, BQ79616_CELL_UV_X100);
}

int32_t bq79616_gpio_code_to_mv(int16_t code)
{
    return code_to_mv(code, BQ79616_GPIO_UV_X100);
}

static bq79616_decode_status_t decode_block(const uint8_t *response,
    uint16_t length, uint8_t devices, uint8_t channels,
    uint16_t expected_register, uint8_t descending,
    int32_t (*convert)(int16_t), int32_t *output)
{
    uint8_t seen[BQ79616_MAX_DEVICES + 1U] = {0};
    uint8_t data[BQ79616_MAX_CELLS * 2U];
    const uint16_t frame_bytes = (uint16_t)(BQ79616_RESPONSE_OVERHEAD_BYTES +
        (uint16_t)channels * 2U);
    uint8_t frame;

    if (response == NULL || output == NULL || devices == 0U ||
        devices > BQ79616_MAX_DEVICES || channels == 0U ||
        channels > BQ79616_MAX_CELLS) {
        return BQ79616_DECODE_INVALID_ARGUMENT;
    }
    if (length != bq79616_stack_response_bytes(devices, channels)) {
        return BQ79616_DECODE_BAD_FRAME;
    }
    for (frame = 0U; frame < devices; frame++) {
        uint8_t device;
        uint16_t address;
        uint8_t channel;
        const bq796xx_response_status_t status = bq796xx_parse_response(
            &response[frame * frame_bytes], frame_bytes, &device, &address,
            data, sizeof(data));

        if (status == BQ796XX_RESPONSE_BAD_CRC) {
            return BQ79616_DECODE_BAD_CRC;
        }
        if (status != BQ796XX_RESPONSE_OK ||
            address != expected_register) {
            return BQ79616_DECODE_BAD_FRAME;
        }
        if (device == 0U || device > devices || seen[device] != 0U) {
            return BQ79616_DECODE_BAD_DEVICE;
        }
        seen[device] = 1U;
        for (channel = 0U; channel < channels; channel++) {
            const uint8_t slot = (descending != 0U) ?
                (uint8_t)(channels - 1U - channel) : channel;
            const int16_t code = (int16_t)(uint16_t)(
                ((uint16_t)data[slot * 2U] << 8) | data[slot * 2U + 1U]);
            output[(uint16_t)(device - 1U) * channels + channel] =
                convert(code);
        }
    }
    return BQ79616_DECODE_OK;
}

bq79616_decode_status_t bq79616_decode_cells(const uint8_t *response,
    uint16_t length, uint8_t devices, uint8_t cells, int32_t *cell_mv)
{
    if (cells < BQ79616_MIN_CELLS || cells > BQ79616_MAX_CELLS) {
        return BQ79616_DECODE_INVALID_ARGUMENT;
    }
    /* The block starts at VCELL<cells>_HI, so the top cell comes first. */
    return decode_block(response, length, devices, cells,
        bq79616_cell_block_address(cells), 1U, bq79616_cell_code_to_mv,
        cell_mv);
}

bq79616_decode_status_t bq79616_decode_gpios(const uint8_t *response,
    uint16_t length, uint8_t devices, uint8_t gpios, int32_t *gpio_mv)
{
    if (gpios > BQ79616_GPIO_COUNT) {
        return BQ79616_DECODE_INVALID_ARGUMENT;
    }
    return decode_block(response, length, devices, gpios,
        BQ79616_REG_GPIO1_HI, 0U, bq79616_gpio_code_to_mv, gpio_mv);
}
