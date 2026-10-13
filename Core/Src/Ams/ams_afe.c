#include "Ams/ams_afe.h"

#include <stddef.h>
#include <string.h>

#include "Peripherals/digital_pins.h"

static ams_afe_t *callback_owner;

static void chip_select(uint8_t selected)
{
    HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN,
        (selected != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void start_transfer(ams_afe_t *afe,
    const bq79600_reader_transfer_t *transfer)
{
    afe->transfer_done = 0U;
    afe->transfer_error = 0U;
    callback_owner = afe;
    chip_select(1U);
    if (HAL_SPI_TransmitReceive_DMA(afe->spi, (uint8_t *)transfer->tx,
        transfer->rx, transfer->length) != HAL_OK) {
        chip_select(0U);
        callback_owner = NULL;
        /* Reported to the reader on the next step. */
        afe->transfer_error = 1U;
    }
}

static ams_afe_status_t error_status(bq79600_reader_error_t error)
{
    switch (error) {
        case BQ79600_READER_ERROR_INVALID_TOPOLOGY:
            return AMS_AFE_NOT_COMMISSIONED;
        case BQ79600_READER_ERROR_READY_TIMEOUT:
        case BQ79600_READER_ERROR_TRANSFER_TIMEOUT:
            return AMS_AFE_TIMEOUT;
        case BQ79600_READER_ERROR_BAD_CRC:
            return AMS_AFE_PEC_ERROR;
        case BQ79600_READER_ERROR_BAD_FRAME:
        case BQ79600_READER_ERROR_BAD_DEVICE:
            return AMS_AFE_INVALID_DATA;
        case BQ79600_READER_ERROR_TRANSFER_FAILED:
        case BQ79600_READER_ERROR_NONE:
        default:
            return AMS_AFE_COMMUNICATION_ERROR;
    }
}

ams_afe_status_t ams_afe_initialize(ams_afe_t *afe, SPI_HandleTypeDef *spi,
    uint8_t devices, uint8_t cells_per_device, uint8_t gpios_per_device)
{
    if (afe == NULL || spi == NULL) {
        return AMS_AFE_INVALID_DATA;
    }
    memset(afe, 0, sizeof(*afe));
    afe->spi = spi;
    if (bq79600_reader_init(&afe->reader, devices, cells_per_device,
        gpios_per_device) == 0U) {
        afe->last_error = afe->reader.last_error;
        return AMS_AFE_NOT_COMMISSIONED;
    }
    afe->initialized = 1U;
    return AMS_AFE_OK;
}

ams_afe_status_t ams_afe_service(ams_afe_t *afe, uint32_t now_ms,
    ams_measurement_t *measurement)
{
    bq79600_reader_inputs_t inputs = {0};
    bq79600_reader_transfer_t transfer;
    bq79600_reader_event_t event;

    if (afe == NULL || measurement == NULL || afe->spi == NULL) {
        return AMS_AFE_INVALID_DATA;
    }
    if (afe->initialized == 0U) {
        return AMS_AFE_NOT_COMMISSIONED;
    }

    inputs.now_ms = now_ms;
    inputs.ready = (HAL_GPIO_ReadPin(AMS_AFE_SPI_READY_GPIO_PORT,
        AMS_AFE_SPI_READY_PIN) == GPIO_PIN_SET) ? 1U : 0U;
    /* The callbacks set these once per transfer, and the next transfer is
     * only started after the reader has consumed them. */
    inputs.transfer_done = afe->transfer_done;
    inputs.transfer_error = afe->transfer_error;
    afe->transfer_done = 0U;
    afe->transfer_error = 0U;

    event = bq79600_reader_step(&afe->reader, &inputs, &transfer);
    switch (event) {
        case BQ79600_READER_START_TRANSFER:
            start_transfer(afe, &transfer);
            return AMS_AFE_IN_PROGRESS;

        case BQ79600_READER_SAMPLE_READY:
            *measurement = afe->reader.measurement;
            afe->sample_counter++;
            return AMS_AFE_NEW_SAMPLE;

        case BQ79600_READER_FAILED:
            afe->last_error = afe->reader.last_error;
            afe->communication_errors++;
            if (afe->last_error == BQ79600_READER_ERROR_TRANSFER_TIMEOUT) {
                callback_owner = NULL;
                (void)HAL_SPI_Abort(afe->spi);
                chip_select(0U);
            }
            return error_status(afe->last_error);

        case BQ79600_READER_WAITING:
        default:
            return AMS_AFE_IN_PROGRESS;
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *spi)
{
    if (callback_owner != NULL && callback_owner->spi == spi) {
        chip_select(0U);
        callback_owner->transfer_done = 1U;
        callback_owner = NULL;
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *spi)
{
    if (callback_owner != NULL && callback_owner->spi == spi) {
        chip_select(0U);
        callback_owner->transfer_error = 1U;
        callback_owner = NULL;
    }
}
