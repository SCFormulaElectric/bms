#ifndef AMS_AFE_H
#define AMS_AFE_H

#include <stdint.h>

#include "Ams/ams_types.h"
#include "Ams/bq79600_reader.h"
#include "stm32f4xx_hal.h"

typedef enum {
    AMS_AFE_OK = 0,
    AMS_AFE_IN_PROGRESS,
    AMS_AFE_NEW_SAMPLE,
    AMS_AFE_NOT_COMMISSIONED,
    AMS_AFE_COMMUNICATION_ERROR,
    AMS_AFE_TIMEOUT,
    AMS_AFE_PEC_ERROR,
    AMS_AFE_INVALID_DATA
} ams_afe_status_t;

/* Hardware glue for the BQ79600 reader: samples SPI_RDY, frames each
 * transfer with nCS, runs it as SPI1 DMA and records completion from the
 * HAL callbacks. All sequencing and decoding is in bq79600_reader. */
typedef struct {
    SPI_HandleTypeDef *spi;
    bq79600_reader_t reader;
    volatile uint8_t transfer_done;
    volatile uint8_t transfer_error;
    uint32_t sample_counter;
    uint32_t communication_errors;
    bq79600_reader_error_t last_error;
    uint8_t initialized;
} ams_afe_t;

/* Call after bq79600_stack_bringup and bq79600_configure_measurements
 * succeed. Returns NOT_COMMISSIONED for an unsupported topology. */
ams_afe_status_t ams_afe_initialize(ams_afe_t *afe, SPI_HandleTypeDef *spi,
    uint8_t devices, uint8_t cells_per_device, uint8_t gpios_per_device);

/* Nonblocking. Poll every AMS_AFE_SERVICE_POLL_MS while IN_PROGRESS. On
 * NEW_SAMPLE, *measurement holds the cell and GPIO voltages (no current,
 * pack voltage or temperatures). An error status means this sample failed;
 * the next call starts recovery and then a fresh sample. */
ams_afe_status_t ams_afe_service(ams_afe_t *afe, uint32_t now_ms,
    ams_measurement_t *measurement);

#endif /* AMS_AFE_H */
