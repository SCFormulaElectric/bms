#include <assert.h>
#include <string.h>

#include "Ams/bq79600_reader.h"
#include "Ams/bq796xx_protocol.h"

/* Builds the stack response the BQ79600 would buffer for one block read:
 * one frame per device, top of stack first, all channels = code. */
static uint16_t stack_response(uint8_t *out, uint8_t devices,
    uint16_t address, uint8_t channels, uint16_t code)
{
    uint16_t position = 0U;
    uint8_t device;

    for (device = devices; device >= 1U; device--) {
        const uint16_t start = position;
        uint8_t channel;
        uint16_t crc;

        out[position++] = (uint8_t)(channels * 2U - 1U);
        out[position++] = device;
        out[position++] = (uint8_t)(address >> 8);
        out[position++] = (uint8_t)address;
        for (channel = 0U; channel < channels; channel++) {
            out[position++] = (uint8_t)(code >> 8);
            out[position++] = (uint8_t)code;
        }
        crc = bq796xx_crc16(&out[start], (uint16_t)(position - start));
        out[position++] = (uint8_t)crc;
        out[position++] = (uint8_t)(crc >> 8);
    }
    return position;
}

typedef struct {
    bq79600_reader_t reader;
    bq79600_reader_inputs_t inputs;
    bq79600_reader_transfer_t transfer;
} fixture_t;

static bq79600_reader_event_t step(fixture_t *f, uint8_t ready,
    uint8_t done)
{
    f->inputs.ready = ready;
    f->inputs.transfer_done = done;
    f->inputs.transfer_error = 0U;
    return bq79600_reader_step(&f->reader, &f->inputs, &f->transfer);
}

/* Drive one block read: command, then response chunks fed from bytes. */
static bq79600_reader_event_t read_block(fixture_t *f, uint16_t address,
    uint8_t data_bytes, const uint8_t *bytes, uint16_t length,
    uint8_t *chunks)
{
    uint8_t expected[8];
    const uint16_t expected_length = bq796xx_build_read(BQ796XX_STACK, 0U,
        address, data_bytes, expected, sizeof(expected));
    uint16_t offset = 0U;
    bq79600_reader_event_t event;

    assert(step(f, 1U, 0U) == BQ79600_READER_START_TRANSFER);
    assert(f->transfer.length == expected_length);
    assert(memcmp(f->transfer.tx, expected, expected_length) == 0);
    f->inputs.now_ms++;
    assert(step(f, 1U, 1U) == BQ79600_READER_WAITING);

    *chunks = 0U;
    for (;;) {
        f->inputs.now_ms++;
        /* Bridge still collecting the stack response. */
        assert(step(f, 0U, 0U) == BQ79600_READER_WAITING);
        f->inputs.now_ms++;
        assert(step(f, 1U, 0U) == BQ79600_READER_START_TRANSFER);
        assert(f->transfer.length <= BQ79600_READER_CHUNK_BYTES);
        assert(f->transfer.tx[0] == 0xFFU);
        memcpy(f->transfer.rx, &bytes[offset], f->transfer.length);
        offset = (uint16_t)(offset + f->transfer.length);
        (*chunks)++;
        f->inputs.now_ms++;
        event = step(f, 0U, 1U);
        if (offset >= length) {
            return event;
        }
        assert(event == BQ79600_READER_WAITING);
    }
}

static void test_one_device_six_cells_four_gpios(void)
{
    fixture_t f;
    uint8_t bytes[BQ79600_READER_MAX_RESPONSE_BYTES];
    uint16_t length;
    uint8_t chunks;
    uint16_t index;

    memset(&f, 0, sizeof(f));
    f.inputs.now_ms = 100U;
    assert(bq79600_reader_init(&f.reader, 1U, 6U, 4U) != 0U);

    length = stack_response(bytes, 1U, bq79616_cell_block_address(6U), 6U,
        0x4B00U);
    assert(length == 18U);
    assert(read_block(&f, bq79616_cell_block_address(6U), 12U, bytes,
        length, &chunks) == BQ79600_READER_WAITING);
    assert(chunks == 1U);

    length = stack_response(bytes, 1U, BQ79616_REG_GPIO1_HI, 4U, 16384U);
    assert(read_block(&f, BQ79616_REG_GPIO1_HI, 8U, bytes, length,
        &chunks) == BQ79600_READER_SAMPLE_READY);

    assert(f.reader.measurement.status == AMS_SAMPLE_VALID);
    assert(f.reader.measurement.valid_cell_count == 6U);
    assert(f.reader.measurement.valid_gpio_count == 4U);
    assert(f.reader.measurement.valid_temperature_count == 0U);
    for (index = 0U; index < 6U; index++) {
        assert(f.reader.measurement.cell_voltage_mv[index] == 3662);
    }
    for (index = 0U; index < 4U; index++) {
        assert(f.reader.measurement.gpio_voltage_mv[index] == 2500);
    }

    /* The next step starts a fresh sample with a new cell command. */
    f.inputs.now_ms++;
    assert(step(&f, 1U, 0U) == BQ79600_READER_START_TRANSFER);
    assert(f.reader.block == BQ79600_READER_BLOCK_CELLS);
}

static void test_full_pack_response_is_read_in_128_byte_chunks(void)
{
    fixture_t f;
    uint8_t bytes[BQ79600_READER_MAX_RESPONSE_BYTES];
    uint16_t length;
    uint8_t chunks;

    memset(&f, 0, sizeof(f));
    assert(bq79600_reader_init(&f.reader, 8U, 12U, 0U) != 0U);
    length = stack_response(bytes, 8U, bq79616_cell_block_address(12U),
        12U, 0x4B00U);
    assert(length == 240U);
    assert(read_block(&f, bq79616_cell_block_address(12U), 24U, bytes,
        length, &chunks) == BQ79600_READER_SAMPLE_READY);
    assert(chunks == 2U);
    assert(f.reader.measurement.valid_cell_count == 96U);
    assert(f.reader.measurement.cell_voltage_mv[95] == 3662);
}

static void test_ready_timeout_fails_then_sends_comm_clear(void)
{
    fixture_t f;

    memset(&f, 0, sizeof(f));
    f.inputs.now_ms = 1000U;
    assert(bq79600_reader_init(&f.reader, 1U, 6U, 4U) != 0U);
    assert(step(&f, 0U, 0U) == BQ79600_READER_WAITING);
    f.inputs.now_ms += BQ79600_READER_READY_TIMEOUT_MS - 1U;
    assert(step(&f, 0U, 0U) == BQ79600_READER_WAITING);
    f.inputs.now_ms++;
    assert(step(&f, 0U, 0U) == BQ79600_READER_FAILED);
    assert(f.reader.last_error == BQ79600_READER_ERROR_READY_TIMEOUT);

    assert(step(&f, 0U, 0U) == BQ79600_READER_START_TRANSFER);
    assert(f.transfer.length == 1U && f.transfer.tx[0] == 0x00U);
    assert(step(&f, 0U, 1U) == BQ79600_READER_WAITING);
    assert(f.reader.state == BQ79600_READER_IDLE);
}

static void test_transfer_timeout_and_error(void)
{
    fixture_t f;

    memset(&f, 0, sizeof(f));
    assert(bq79600_reader_init(&f.reader, 1U, 6U, 4U) != 0U);
    assert(step(&f, 1U, 0U) == BQ79600_READER_START_TRANSFER);
    f.inputs.now_ms += BQ79600_READER_TRANSFER_TIMEOUT_MS;
    assert(step(&f, 1U, 0U) == BQ79600_READER_FAILED);
    assert(f.reader.last_error == BQ79600_READER_ERROR_TRANSFER_TIMEOUT);

    assert(bq79600_reader_init(&f.reader, 1U, 6U, 4U) != 0U);
    assert(step(&f, 1U, 0U) == BQ79600_READER_START_TRANSFER);
    f.inputs.transfer_error = 1U;
    assert(bq79600_reader_step(&f.reader, &f.inputs, &f.transfer) ==
        BQ79600_READER_FAILED);
    assert(f.reader.last_error == BQ79600_READER_ERROR_TRANSFER_FAILED);
}

static void test_bad_crc_fails_sample(void)
{
    fixture_t f;
    uint8_t bytes[BQ79600_READER_MAX_RESPONSE_BYTES];
    uint16_t length;
    uint8_t chunks;

    memset(&f, 0, sizeof(f));
    assert(bq79600_reader_init(&f.reader, 1U, 6U, 4U) != 0U);
    length = stack_response(bytes, 1U, bq79616_cell_block_address(6U), 6U,
        0x4B00U);
    bytes[6] ^= 0x01U;
    assert(read_block(&f, bq79616_cell_block_address(6U), 12U, bytes,
        length, &chunks) == BQ79600_READER_FAILED);
    assert(f.reader.last_error == BQ79600_READER_ERROR_BAD_CRC);
    assert(f.reader.measurement.status != AMS_SAMPLE_VALID);
}

static void test_invalid_topology_is_rejected(void)
{
    fixture_t f;

    memset(&f, 0, sizeof(f));
    assert(bq79600_reader_init(&f.reader, 0U, 6U, 4U) == 0U);
    assert(step(&f, 1U, 0U) == BQ79600_READER_FAILED);
    assert(f.reader.last_error == BQ79600_READER_ERROR_INVALID_TOPOLOGY);
    assert(bq79600_reader_init(&f.reader, 1U, 5U, 4U) == 0U);
    assert(bq79600_reader_init(&f.reader, 1U, 6U, 5U) == 0U);
}

int main(void)
{
    test_one_device_six_cells_four_gpios();
    test_full_pack_response_is_read_in_128_byte_chunks();
    test_ready_timeout_fails_then_sends_comm_clear();
    test_transfer_timeout_and_error();
    test_bad_crc_fails_sample();
    test_invalid_topology_is_rejected();
    return 0;
}
