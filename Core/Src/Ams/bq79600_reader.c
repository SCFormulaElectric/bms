#include "Ams/bq79600_reader.h"

#include <stddef.h>
#include <string.h>

#include "Ams/bq796xx_protocol.h"

static uint8_t expired(uint32_t now_ms, uint32_t deadline_ms)
{
    return ((int32_t)(now_ms - deadline_ms) >= 0) ? 1U : 0U;
}

static uint8_t topology_valid(uint8_t devices, uint8_t cells,
    uint8_t gpios)
{
    return (devices != 0U && devices <= AMS_MAX_SEGMENTS &&
        cells >= BQ79616_MIN_CELLS && cells <= AMS_MAX_CELLS_PER_SEGMENT &&
        gpios <= AMS_MAX_TEMPS_PER_SEGMENT) ? 1U : 0U;
}

uint8_t bq79600_reader_init(bq79600_reader_t *reader, uint8_t devices,
    uint8_t cells_per_device, uint8_t gpios_per_device)
{
    if (reader == NULL) {
        return 0U;
    }
    memset(reader, 0, sizeof(*reader));
    memset(reader->dummy, 0xFF, sizeof(reader->dummy));
    reader->devices = devices;
    reader->cells_per_device = cells_per_device;
    reader->gpios_per_device = gpios_per_device;
    reader->state = BQ79600_READER_IDLE;
    if (topology_valid(devices, cells_per_device, gpios_per_device) == 0U) {
        reader->last_error = BQ79600_READER_ERROR_INVALID_TOPOLOGY;
        return 0U;
    }
    return 1U;
}

static uint8_t block_channels(const bq79600_reader_t *reader)
{
    return (reader->block == BQ79600_READER_BLOCK_CELLS) ?
        reader->cells_per_device : reader->gpios_per_device;
}

static void begin_block(bq79600_reader_t *reader,
    bq79600_reader_block_t block, uint32_t now_ms)
{
    const uint16_t address = (block == BQ79600_READER_BLOCK_CELLS) ?
        bq79616_cell_block_address(reader->cells_per_device) :
        BQ79616_REG_GPIO1_HI;

    reader->block = block;
    reader->command_length = bq796xx_build_read(BQ796XX_STACK, 0U, address,
        (uint8_t)(block_channels(reader) * 2U), reader->command,
        sizeof(reader->command));
    reader->response_bytes = bq79616_stack_response_bytes(reader->devices,
        block_channels(reader));
    reader->offset = 0U;
    reader->state = BQ79600_READER_WAIT_COMMAND_READY;
    reader->deadline_ms = now_ms + BQ79600_READER_READY_TIMEOUT_MS;
}

static bq79600_reader_event_t fail(bq79600_reader_t *reader,
    bq79600_reader_error_t error)
{
    reader->last_error = error;
    reader->state = BQ79600_READER_COMM_CLEAR;
    return BQ79600_READER_FAILED;
}

static bq79600_reader_error_t decode_error(bq79616_decode_status_t status)
{
    switch (status) {
        case BQ79616_DECODE_OK:
            return BQ79600_READER_ERROR_NONE;
        case BQ79616_DECODE_BAD_CRC:
            return BQ79600_READER_ERROR_BAD_CRC;
        case BQ79616_DECODE_BAD_DEVICE:
            return BQ79600_READER_ERROR_BAD_DEVICE;
        case BQ79616_DECODE_INVALID_ARGUMENT:
        case BQ79616_DECODE_BAD_FRAME:
        default:
            return BQ79600_READER_ERROR_BAD_FRAME;
    }
}

/* Decode the completed response; returns SAMPLE_READY after the last block. */
static bq79600_reader_event_t finish_block(bq79600_reader_t *reader,
    uint32_t now_ms)
{
    bq79600_reader_error_t error;
    ams_measurement_t *measurement = &reader->measurement;

    if (reader->block == BQ79600_READER_BLOCK_CELLS) {
        error = decode_error(bq79616_decode_cells(reader->response,
            reader->response_bytes, reader->devices,
            reader->cells_per_device, measurement->cell_voltage_mv));
        if (error != BQ79600_READER_ERROR_NONE) {
            return fail(reader, error);
        }
        if (reader->gpios_per_device != 0U) {
            begin_block(reader, BQ79600_READER_BLOCK_GPIOS, now_ms);
            return BQ79600_READER_WAITING;
        }
    } else {
        error = decode_error(bq79616_decode_gpios(reader->response,
            reader->response_bytes, reader->devices,
            reader->gpios_per_device, measurement->gpio_voltage_mv));
        if (error != BQ79600_READER_ERROR_NONE) {
            return fail(reader, error);
        }
    }

    measurement->valid_cell_count =
        (uint16_t)reader->devices * reader->cells_per_device;
    measurement->valid_gpio_count =
        (uint16_t)reader->devices * reader->gpios_per_device;
    measurement->valid_temperature_count = 0U;
    measurement->timestamp_ms = now_ms;
    measurement->status = AMS_SAMPLE_VALID;
    reader->last_error = BQ79600_READER_ERROR_NONE;
    reader->state = BQ79600_READER_IDLE;
    return BQ79600_READER_SAMPLE_READY;
}

static bq79600_reader_event_t request(bq79600_reader_t *reader,
    bq79600_reader_transfer_t *transfer, const uint8_t *tx, uint8_t *rx,
    uint16_t length, bq79600_reader_state_t next, uint32_t now_ms)
{
    transfer->tx = tx;
    transfer->rx = rx;
    transfer->length = length;
    reader->state = next;
    reader->deadline_ms = now_ms + BQ79600_READER_TRANSFER_TIMEOUT_MS;
    return BQ79600_READER_START_TRANSFER;
}

bq79600_reader_event_t bq79600_reader_step(bq79600_reader_t *reader,
    const bq79600_reader_inputs_t *inputs,
    bq79600_reader_transfer_t *transfer)
{
    uint32_t now_ms;

    if (reader == NULL || inputs == NULL || transfer == NULL) {
        return BQ79600_READER_FAILED;
    }
    if (topology_valid(reader->devices, reader->cells_per_device,
        reader->gpios_per_device) == 0U) {
        reader->last_error = BQ79600_READER_ERROR_INVALID_TOPOLOGY;
        return BQ79600_READER_FAILED;
    }
    now_ms = inputs->now_ms;
    memset(transfer, 0, sizeof(*transfer));

    switch (reader->state) {
        case BQ79600_READER_IDLE:
            memset(&reader->measurement, 0, sizeof(reader->measurement));
            begin_block(reader, BQ79600_READER_BLOCK_CELLS, now_ms);
            /* Fall through: the bridge may already be ready. */
            /* fall through */
        case BQ79600_READER_WAIT_COMMAND_READY:
            if (inputs->ready != 0U) {
                return request(reader, transfer, reader->command,
                    reader->command_rx, reader->command_length,
                    BQ79600_READER_COMMAND_TRANSFER, now_ms);
            }
            if (expired(now_ms, reader->deadline_ms) != 0U) {
                return fail(reader, BQ79600_READER_ERROR_READY_TIMEOUT);
            }
            return BQ79600_READER_WAITING;

        case BQ79600_READER_COMMAND_TRANSFER:
        case BQ79600_READER_RESPONSE_TRANSFER:
            if (inputs->transfer_error != 0U) {
                return fail(reader, BQ79600_READER_ERROR_TRANSFER_FAILED);
            }
            if (inputs->transfer_done == 0U) {
                return (expired(now_ms, reader->deadline_ms) != 0U) ?
                    fail(reader, BQ79600_READER_ERROR_TRANSFER_TIMEOUT) :
                    BQ79600_READER_WAITING;
            }
            if (reader->state == BQ79600_READER_RESPONSE_TRANSFER) {
                reader->offset = (uint16_t)(reader->offset + reader->chunk);
                if (reader->offset >= reader->response_bytes) {
                    return finish_block(reader, now_ms);
                }
            }
            /* Command sent, or more response left: wait for SPI_RDY. The
             * level sampled with this step predates the transfer, so it is
             * not trusted until the next step. */
            reader->state = BQ79600_READER_WAIT_RESPONSE_READY;
            reader->deadline_ms = now_ms + BQ79600_READER_READY_TIMEOUT_MS;
            return BQ79600_READER_WAITING;

        case BQ79600_READER_WAIT_RESPONSE_READY:
            if (inputs->ready != 0U) {
                reader->chunk = (uint16_t)(reader->response_bytes -
                    reader->offset);
                if (reader->chunk > BQ79600_READER_CHUNK_BYTES) {
                    reader->chunk = BQ79600_READER_CHUNK_BYTES;
                }
                return request(reader, transfer, reader->dummy,
                    &reader->response[reader->offset], reader->chunk,
                    BQ79600_READER_RESPONSE_TRANSFER, now_ms);
            }
            if (expired(now_ms, reader->deadline_ms) != 0U) {
                return fail(reader, BQ79600_READER_ERROR_READY_TIMEOUT);
            }
            return BQ79600_READER_WAITING;

        case BQ79600_READER_COMM_CLEAR:
            /* A single zero byte resets the bridge's SPI frame parser. */
            reader->comm_clear[0] = 0U;
            return request(reader, transfer, reader->comm_clear,
                reader->command_rx, 1U,
                BQ79600_READER_COMM_CLEAR_TRANSFER, now_ms);

        case BQ79600_READER_COMM_CLEAR_TRANSFER:
            /* Best effort: whatever happens, start over with a new sample. */
            if (inputs->transfer_done == 0U && inputs->transfer_error == 0U &&
                expired(now_ms, reader->deadline_ms) == 0U) {
                return BQ79600_READER_WAITING;
            }
            reader->state = BQ79600_READER_IDLE;
            return BQ79600_READER_WAITING;

        default:
            return fail(reader, BQ79600_READER_ERROR_BAD_FRAME);
    }
}
