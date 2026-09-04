#include "Ams/ams_afe.h"

#include <stddef.h>
#include <string.h>

#include "Peripherals/digital_pins.h"

static ams_afe_t *callback_owner;

static ams_afe_status_t fail_pipeline(ams_afe_t *afe,
    ams_afe_status_t status);

static uint8_t deadline_expired(uint32_t now_ms, uint32_t deadline_ms)
{
    return ((int32_t)(now_ms - deadline_ms) >= 0) ? 1U : 0U;
}

static void chip_select(uint8_t selected)
{
    HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN,
        (selected != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static ams_afe_status_t begin_dma(ams_afe_t *afe,
    ams_afe_transaction_t transaction,
    ams_afe_pipeline_state_t state_after_dma, uint32_t now_ms)
{
    uint16_t length = 0U;
    ams_afe_status_t status;

    afe->dma_buffer ^= 1U;
    memset(afe->tx_buffer[afe->dma_buffer], 0,
        sizeof(afe->tx_buffer[afe->dma_buffer]));
    memset(afe->rx_buffer[afe->dma_buffer], 0,
        sizeof(afe->rx_buffer[afe->dma_buffer]));
    status = afe->profile->prepare(transaction,
        afe->tx_buffer[afe->dma_buffer], AMS_AFE_DMA_BUFFER_BYTES, &length);
    if (status != AMS_AFE_OK || length == 0U ||
        length > AMS_AFE_DMA_BUFFER_BYTES) {
        return fail_pipeline(afe, AMS_AFE_INVALID_DATA);
    }

    afe->dma_complete = 0U;
    afe->dma_error = 0U;
    afe->dma_length = length;
    afe->pending_transaction = transaction;
    afe->state_after_dma = state_after_dma;
    afe->deadline_ms = now_ms + AMS_AFE_DMA_TIMEOUT_MS;
    callback_owner = afe;
    chip_select(1U);
    if (HAL_SPI_TransmitReceive_DMA(afe->spi,
        afe->tx_buffer[afe->dma_buffer], afe->rx_buffer[afe->dma_buffer],
        length) != HAL_OK) {
        chip_select(0U);
        callback_owner = NULL;
        return fail_pipeline(afe, AMS_AFE_COMMUNICATION_ERROR);
    }
    afe->state = AMS_AFE_PIPELINE_DMA_WAIT;
    return AMS_AFE_IN_PROGRESS;
}

static void publish_measurement(ams_afe_t *afe)
{
    afe->publication_sequence++;
    __DMB();
    afe->published_measurement = afe->working_measurement;
    afe->sample_counter++;
    __DMB();
    afe->publication_sequence++;
    afe->working_measurement ^= 1U;
}

static ams_afe_status_t fail_pipeline(ams_afe_t *afe,
    ams_afe_status_t status)
{
    chip_select(0U);
    afe->communication_errors++;
    afe->terminal_status = status;
    afe->state = AMS_AFE_PIPELINE_FAULT;
    return status;
}

void ams_afe_reset(ams_afe_t *afe)
{
    if (afe != NULL) {
        memset(afe, 0, sizeof(*afe));
        afe->state = AMS_AFE_PIPELINE_RESET;
        afe->terminal_status = AMS_AFE_NOT_COMMISSIONED;
    }
}

ams_afe_status_t ams_afe_initialize(ams_afe_t *afe,
    SPI_HandleTypeDef *spi, const ams_afe_profile_t *profile)
{
    if (afe == NULL || spi == NULL) {
        return AMS_AFE_INVALID_DATA;
    }
    ams_afe_reset(afe);
    afe->spi = spi;
    afe->profile = profile;
    if (profile == NULL || profile->commissioned == 0U ||
        profile->expected_cell_count == 0U ||
        profile->expected_cell_count > AMS_MAX_CELLS ||
        profile->expected_temperature_count == 0U ||
        profile->expected_temperature_count > AMS_MAX_TEMPERATURES ||
        profile->prepare == NULL || profile->decode == NULL) {
        afe->state = AMS_AFE_PIPELINE_FAULT;
        afe->terminal_status = AMS_AFE_NOT_COMMISSIONED;
        return AMS_AFE_NOT_COMMISSIONED;
    }
    afe->initialized = 1U;
    afe->state = AMS_AFE_PIPELINE_START_CELLS;
    afe->terminal_status = AMS_AFE_OK;
    return AMS_AFE_OK;
}

ams_afe_status_t ams_afe_service(ams_afe_t *afe, uint32_t now_ms)
{
    ams_afe_status_t status;
    ams_measurement_t *working;

    if (afe == NULL || afe->spi == NULL) {
        return AMS_AFE_INVALID_DATA;
    }
    if (afe->state == AMS_AFE_PIPELINE_FAULT) {
        return afe->terminal_status;
    }
    if (afe->initialized == 0U || afe->profile == NULL) {
        return fail_pipeline(afe, AMS_AFE_NOT_COMMISSIONED);
    }
    working = &afe->measurement_buffer[afe->working_measurement];

    switch (afe->state) {
        case AMS_AFE_PIPELINE_START_CELLS:
            memset(working, 0, sizeof(*working));
            working->timestamp_ms = now_ms;
            return begin_dma(afe, AMS_AFE_TRANSACTION_START_CELLS,
                AMS_AFE_PIPELINE_WAIT_CELL_CONVERSION, now_ms);

        case AMS_AFE_PIPELINE_WAIT_CELL_CONVERSION:
            if (deadline_expired(now_ms, afe->deadline_ms) != 0U) {
                afe->state = AMS_AFE_PIPELINE_READ_CELLS;
            }
            return AMS_AFE_IN_PROGRESS;

        case AMS_AFE_PIPELINE_READ_CELLS:
            return begin_dma(afe, AMS_AFE_TRANSACTION_READ_CELLS,
                AMS_AFE_PIPELINE_START_TEMPERATURES, now_ms);

        case AMS_AFE_PIPELINE_START_TEMPERATURES:
            return begin_dma(afe, AMS_AFE_TRANSACTION_START_TEMPERATURES,
                AMS_AFE_PIPELINE_WAIT_TEMPERATURE_CONVERSION, now_ms);

        case AMS_AFE_PIPELINE_WAIT_TEMPERATURE_CONVERSION:
            if (deadline_expired(now_ms, afe->deadline_ms) != 0U) {
                afe->state = AMS_AFE_PIPELINE_READ_TEMPERATURES;
            }
            return AMS_AFE_IN_PROGRESS;

        case AMS_AFE_PIPELINE_READ_TEMPERATURES:
            return begin_dma(afe, AMS_AFE_TRANSACTION_READ_TEMPERATURES,
                AMS_AFE_PIPELINE_READ_DIAGNOSTICS, now_ms);

        case AMS_AFE_PIPELINE_READ_DIAGNOSTICS:
            return begin_dma(afe, AMS_AFE_TRANSACTION_READ_DIAGNOSTICS,
                AMS_AFE_PIPELINE_START_CELLS, now_ms);

        case AMS_AFE_PIPELINE_DMA_WAIT:
            if (afe->dma_error != 0U) {
                return fail_pipeline(afe, AMS_AFE_COMMUNICATION_ERROR);
            }
            if (afe->dma_complete == 0U) {
                if (deadline_expired(now_ms, afe->deadline_ms) != 0U) {
                    (void)HAL_SPI_Abort_IT(afe->spi);
                    return fail_pipeline(afe, AMS_AFE_TIMEOUT);
                }
                return AMS_AFE_IN_PROGRESS;
            }
            afe->dma_complete = 0U;
            status = afe->profile->decode(afe->pending_transaction,
                afe->rx_buffer[afe->dma_buffer], afe->dma_length, working);
            if (status != AMS_AFE_OK) {
                return fail_pipeline(afe, status);
            }
            afe->state = afe->state_after_dma;
            if (afe->pending_transaction ==
                AMS_AFE_TRANSACTION_START_CELLS) {
                afe->deadline_ms = now_ms +
                    afe->profile->cell_conversion_ms;
            } else if (afe->pending_transaction ==
                AMS_AFE_TRANSACTION_START_TEMPERATURES) {
                afe->deadline_ms = now_ms +
                    afe->profile->temperature_conversion_ms;
            } else if (afe->pending_transaction ==
                AMS_AFE_TRANSACTION_READ_DIAGNOSTICS) {
                if (working->status != AMS_SAMPLE_VALID ||
                    working->valid_cell_count !=
                        afe->profile->expected_cell_count ||
                    working->valid_temperature_count !=
                        afe->profile->expected_temperature_count) {
                    return fail_pipeline(afe, AMS_AFE_INVALID_DATA);
                }
                working->timestamp_ms = now_ms;
                publish_measurement(afe);
                return AMS_AFE_NEW_SAMPLE;
            }
            return AMS_AFE_IN_PROGRESS;

        case AMS_AFE_PIPELINE_RESET:
        default:
            return fail_pipeline(afe, AMS_AFE_INVALID_DATA);
    }
}

ams_afe_status_t ams_afe_fetch_latest(const ams_afe_t *afe,
    ams_measurement_t *measurement, uint32_t *sample_counter)
{
    uint32_t sequence_before;
    uint32_t sequence_after;
    uint8_t attempt;
    uint8_t index;

    if (afe == NULL || measurement == NULL || sample_counter == NULL ||
        afe->sample_counter == 0U) {
        return AMS_AFE_INVALID_DATA;
    }
    for (attempt = 0U; attempt < 3U; attempt++) {
        sequence_before = afe->publication_sequence;
        if ((sequence_before & 1U) != 0U) {
            continue;
        }
        __DMB();
        index = afe->published_measurement;
        *measurement = afe->measurement_buffer[index];
        *sample_counter = afe->sample_counter;
        __DMB();
        sequence_after = afe->publication_sequence;
        if (sequence_before == sequence_after &&
            (sequence_after & 1U) == 0U) {
            return AMS_AFE_OK;
        }
    }
    return AMS_AFE_IN_PROGRESS;
}

const ams_afe_profile_t *ams_afe_default_profile(void)
{
    return NULL;
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *spi)
{
    if (callback_owner != NULL && callback_owner->spi == spi) {
        chip_select(0U);
        callback_owner->dma_complete = 1U;
        callback_owner = NULL;
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *spi)
{
    if (callback_owner != NULL && callback_owner->spi == spi) {
        chip_select(0U);
        callback_owner->dma_error = 1U;
        callback_owner = NULL;
    }
}

void HAL_SPI_AbortCpltCallback(SPI_HandleTypeDef *spi)
{
    if (callback_owner != NULL && callback_owner->spi == spi) {
        chip_select(0U);
        callback_owner->dma_error = 1U;
        callback_owner = NULL;
    }
}
