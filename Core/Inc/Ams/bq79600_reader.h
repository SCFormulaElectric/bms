#ifndef BQ79600_READER_H
#define BQ79600_READER_H

#include <stdint.h>

#include "Ams/ams_types.h"
#include "Ams/bq79616_measurement.h"

/* Nonblocking, HAL-free sequencer for reading BQ79616 measurements through
 * the BQ79600 SPI bridge. A BQ79600 read is not one SPI transfer:
 *
 *   1. wait for SPI_RDY (bridge can accept a command), send the command;
 *   2. wait for SPI_RDY again (the stack response is buffered);
 *   3. clock the response out with dummy bytes, at most 128 bytes per
 *      SPI_RDY, repeating 2-3 until the whole response is read.
 *
 * The caller owns the hardware: each step it reports the SPI_RDY level and
 * whether the last transfer finished or failed, and performs any transfer
 * the step requests (chip select low, full-duplex DMA, chip select high). */

#define BQ79600_READER_CHUNK_BYTES          128U
#define BQ79600_READER_READY_TIMEOUT_MS       5U
#define BQ79600_READER_TRANSFER_TIMEOUT_MS    5U
#define BQ79600_READER_MAX_RESPONSE_BYTES \
    (AMS_MAX_SEGMENTS * (BQ79616_RESPONSE_OVERHEAD_BYTES + \
        2U * AMS_MAX_CELLS_PER_SEGMENT))

typedef enum {
    BQ79600_READER_IDLE = 0,
    BQ79600_READER_WAIT_COMMAND_READY,
    BQ79600_READER_COMMAND_TRANSFER,
    BQ79600_READER_WAIT_RESPONSE_READY,
    BQ79600_READER_RESPONSE_TRANSFER,
    BQ79600_READER_COMM_CLEAR,
    BQ79600_READER_COMM_CLEAR_TRANSFER
} bq79600_reader_state_t;

typedef enum {
    BQ79600_READER_BLOCK_CELLS = 0,
    BQ79600_READER_BLOCK_GPIOS
} bq79600_reader_block_t;

typedef enum {
    BQ79600_READER_ERROR_NONE = 0,
    BQ79600_READER_ERROR_INVALID_TOPOLOGY,
    BQ79600_READER_ERROR_READY_TIMEOUT,
    BQ79600_READER_ERROR_TRANSFER_TIMEOUT,
    BQ79600_READER_ERROR_TRANSFER_FAILED,
    BQ79600_READER_ERROR_BAD_CRC,
    BQ79600_READER_ERROR_BAD_FRAME,
    BQ79600_READER_ERROR_BAD_DEVICE
} bq79600_reader_error_t;

typedef enum {
    BQ79600_READER_WAITING = 0,     /* nothing to do this step */
    BQ79600_READER_START_TRANSFER,  /* perform *transfer now */
    BQ79600_READER_SAMPLE_READY,    /* reader->measurement is complete */
    BQ79600_READER_FAILED           /* see reader->last_error */
} bq79600_reader_event_t;

typedef struct {
    uint32_t now_ms;
    uint8_t ready;            /* SPI_RDY pin is high */
    uint8_t transfer_done;    /* the requested transfer completed */
    uint8_t transfer_error;   /* the requested transfer failed */
} bq79600_reader_inputs_t;

typedef struct {
    const uint8_t *tx;
    uint8_t *rx;
    uint16_t length;
} bq79600_reader_transfer_t;

typedef struct {
    uint8_t devices;
    uint8_t cells_per_device;
    uint8_t gpios_per_device;
    bq79600_reader_state_t state;
    bq79600_reader_block_t block;
    bq79600_reader_error_t last_error;
    uint32_t deadline_ms;
    uint16_t response_bytes;
    uint16_t offset;
    uint16_t chunk;
    uint16_t command_length;
    uint8_t command[8];
    uint8_t command_rx[8];
    uint8_t comm_clear[1];
    uint8_t dummy[BQ79600_READER_CHUNK_BYTES];
    uint8_t response[BQ79600_READER_MAX_RESPONSE_BYTES];
    ams_measurement_t measurement;
} bq79600_reader_t;

/* Returns 0 if the topology does not fit the measurement arrays or the
 * BQ79616 limits; the reader then reports INVALID_TOPOLOGY on every step. */
uint8_t bq79600_reader_init(bq79600_reader_t *reader, uint8_t devices,
    uint8_t cells_per_device, uint8_t gpios_per_device);

/* Advance the sequencer once. After FAILED the reader sends a one-byte
 * communication clear on the following steps, then starts a fresh sample.
 * A FAILED with TRANSFER_TIMEOUT means the caller must abort its transfer. */
bq79600_reader_event_t bq79600_reader_step(bq79600_reader_t *reader,
    const bq79600_reader_inputs_t *inputs,
    bq79600_reader_transfer_t *transfer);

#endif /* BQ79600_READER_H */
