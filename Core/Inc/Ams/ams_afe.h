#ifndef AMS_AFE_H
#define AMS_AFE_H

#include <stdint.h>

#include "Ams/ams_types.h"
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

typedef enum {
    AMS_AFE_TRANSACTION_START_CELLS = 0,
    AMS_AFE_TRANSACTION_READ_CELLS,
    AMS_AFE_TRANSACTION_START_TEMPERATURES,
    AMS_AFE_TRANSACTION_READ_TEMPERATURES,
    AMS_AFE_TRANSACTION_READ_DIAGNOSTICS
} ams_afe_transaction_t;

typedef ams_afe_status_t (*ams_afe_prepare_fn)(
    ams_afe_transaction_t transaction, uint8_t *tx_data,
    uint16_t capacity, uint16_t *length);

typedef ams_afe_status_t (*ams_afe_decode_fn)(
    ams_afe_transaction_t transaction, const uint8_t *rx_data,
    uint16_t length, ams_measurement_t *measurement);

typedef struct {
    uint8_t commissioned;
    uint32_t cell_conversion_ms;
    uint32_t temperature_conversion_ms;
    ams_afe_prepare_fn prepare;
    ams_afe_decode_fn decode;
} ams_afe_profile_t;

typedef enum {
    AMS_AFE_PIPELINE_RESET = 0,
    AMS_AFE_PIPELINE_START_CELLS,
    AMS_AFE_PIPELINE_WAIT_CELL_CONVERSION,
    AMS_AFE_PIPELINE_READ_CELLS,
    AMS_AFE_PIPELINE_START_TEMPERATURES,
    AMS_AFE_PIPELINE_WAIT_TEMPERATURE_CONVERSION,
    AMS_AFE_PIPELINE_READ_TEMPERATURES,
    AMS_AFE_PIPELINE_READ_DIAGNOSTICS,
    AMS_AFE_PIPELINE_DMA_WAIT,
    AMS_AFE_PIPELINE_FAULT
} ams_afe_pipeline_state_t;

typedef struct {
    SPI_HandleTypeDef *spi;
    const ams_afe_profile_t *profile;
    ams_afe_pipeline_state_t state;
    ams_afe_pipeline_state_t state_after_dma;
    ams_afe_transaction_t pending_transaction;
    uint8_t tx_buffer[2][AMS_AFE_DMA_BUFFER_BYTES];
    uint8_t rx_buffer[2][AMS_AFE_DMA_BUFFER_BYTES];
    ams_measurement_t measurement_buffer[2];
    volatile uint32_t publication_sequence;
    volatile uint8_t published_measurement;
    uint8_t working_measurement;
    uint8_t dma_buffer;
    volatile uint8_t dma_complete;
    volatile uint8_t dma_error;
    uint16_t dma_length;
    uint32_t deadline_ms;
    uint32_t sample_counter;
    uint32_t communication_errors;
    ams_afe_status_t terminal_status;
    uint8_t initialized;
} ams_afe_t;

void ams_afe_reset(ams_afe_t *afe);
ams_afe_status_t ams_afe_initialize(ams_afe_t *afe,
    SPI_HandleTypeDef *spi, const ams_afe_profile_t *profile);
ams_afe_status_t ams_afe_service(ams_afe_t *afe, uint32_t now_ms);
ams_afe_status_t ams_afe_fetch_latest(const ams_afe_t *afe,
    ams_measurement_t *measurement, uint32_t *sample_counter);

/* Returns NULL until an IC-specific profile is selected and implemented. */
const ams_afe_profile_t *ams_afe_default_profile(void);

#endif /* AMS_AFE_H */
