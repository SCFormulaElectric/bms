#include "Tasks/Status/uart_broadcast_task.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "Ams/bq79600_bridge.h"
#include "Ams/bq79600_reader.h"

typedef struct {
    uint32_t now_ms;
    uint8_t bringup_status;
    uint8_t bringup_failed_step;
    uint8_t configured_devices;
    uint8_t verified_devices;
    uint8_t bridge_device_config;
    uint32_t bringup_attempts;
    bq79600_comm_diag_t diag;
    uint32_t afe_samples;
    uint32_t afe_errors;
    uint8_t afe_last_error;
    ams_state_t state;
    uint32_t active_faults;
    uint32_t latched_faults;
    uint16_t soc_permille;
    ams_measurement_t measurement;
} uart_snapshot_t;

static const char *const bringup_status_names[] = {
    [BQ79600_BRINGUP_OK] = "OK",
    [BQ79600_BRINGUP_INVALID_ARGUMENT] = "INVALID_ARGUMENT",
    [BQ79600_BRINGUP_TIMER_UNAVAILABLE] = "TIMER_UNAVAILABLE",
    [BQ79600_BRINGUP_RDY_TIMEOUT] = "RDY_TIMEOUT",
    [BQ79600_BRINGUP_SPI_ERROR] = "SPI_ERROR",
    [BQ79600_BRINGUP_RESPONSE_FORMAT] = "RESPONSE_FORMAT",
    [BQ79600_BRINGUP_RESPONSE_CRC] = "RESPONSE_CRC",
    [BQ79600_BRINGUP_ADDRESS_MISMATCH] = "ADDRESS_MISMATCH",
    [BQ79600_BRINGUP_DEVICE_CONFIG_MISMATCH] = "DEVICE_CONFIG_MISMATCH"
};

static const char *const bringup_step_names[] = {
    [BQ79600_STEP_NONE] = "NONE",
    [BQ79600_STEP_GPIO_WAKE] = "GPIO_WAKE",
    [BQ79600_STEP_STACK_WAKE] = "STACK_WAKE",
    [BQ79600_STEP_WRITE_DLL_SYNC] = "WRITE_DLL_SYNC",
    [BQ79600_STEP_ENABLE_AUTO_ADDRESS] = "ENABLE_AUTO_ADDRESS",
    [BQ79600_STEP_ASSIGN_ADDRESSES] = "ASSIGN_ADDRESSES",
    [BQ79600_STEP_CONFIGURE_STACK] = "CONFIGURE_STACK",
    [BQ79600_STEP_CONFIGURE_TOP] = "CONFIGURE_TOP",
    [BQ79600_STEP_READ_DLL_SYNC] = "READ_DLL_SYNC",
    [BQ79600_STEP_VERIFY_ADDRESSES] = "VERIFY_ADDRESSES",
    [BQ79600_STEP_VERIFY_BRIDGE] = "VERIFY_BRIDGE",
    [BQ79600_STEP_CONFIGURE_MEASUREMENT] = "CONFIGURE_MEASUREMENT",
    [BQ79600_STEP_VERIFY_MEASUREMENT] = "VERIFY_MEASUREMENT",
    [BQ79600_STEP_DISABLE_COMM_TIMEOUT] = "DISABLE_COMM_TIMEOUT"
};

static const char *const reader_error_names[] = {
    [BQ79600_READER_ERROR_NONE] = "NONE",
    [BQ79600_READER_ERROR_INVALID_TOPOLOGY] = "INVALID_TOPOLOGY",
    [BQ79600_READER_ERROR_READY_TIMEOUT] = "READY_TIMEOUT",
    [BQ79600_READER_ERROR_TRANSFER_TIMEOUT] = "TRANSFER_TIMEOUT",
    [BQ79600_READER_ERROR_TRANSFER_FAILED] = "TRANSFER_FAILED",
    [BQ79600_READER_ERROR_BAD_CRC] = "BAD_CRC",
    [BQ79600_READER_ERROR_BAD_FRAME] = "BAD_FRAME",
    [BQ79600_READER_ERROR_BAD_DEVICE] = "BAD_DEVICE"
};

static const char *const state_names[] = {
    [AMS_STATE_INIT] = "INIT",
    [AMS_STATE_SELF_TEST] = "SELF_TEST",
    [AMS_STATE_STANDBY] = "STANDBY",
    [AMS_STATE_PRECHARGE] = "PRECHARGE",
    [AMS_STATE_ACTIVE] = "ACTIVE",
    [AMS_STATE_CHARGING] = "CHARGING",
    [AMS_STATE_BALANCING] = "BALANCING",
    [AMS_STATE_FAULT_LATCHED] = "FAULT_LATCHED",
    [AMS_STATE_SHUTDOWN] = "SHUTDOWN"
};

#define NAME_OF(table, value) \
    lookup_name((table), sizeof(table) / sizeof((table)[0]), (value))

static TaskHandle_t volatile tx_waiter;
static char report[UART_BROADCAST_BUFFER_BYTES];
static size_t report_length;
static uart_snapshot_t snapshot;

static const char *lookup_name(const char *const *names, size_t count,
    uint32_t value)
{
    return (value < count && names[value] != NULL) ? names[value] : "?";
}

static void append(const char *format, ...)
{
    const size_t capacity = sizeof(report) - report_length;
    va_list args;
    int written;

    if (capacity <= 1U) {
        return;
    }
    va_start(args, format);
    written = vsnprintf(&report[report_length], capacity, format, args);
    va_end(args);
    if (written < 0) {
        return;
    }
    /* On overflow keep the truncated text; later appends are dropped. */
    report_length += ((size_t)written < capacity) ? (size_t)written :
        capacity - 1U;
}

/* Prints a milli-unit value as a signed decimal without printf float
 * support, which newlib-nano omits by default. */
static void append_milli(const char *label, uint16_t index, int32_t milli,
    const char *unit)
{
    const uint32_t magnitude = (milli < 0) ?
        (uint32_t)(-(int64_t)milli) : (uint32_t)milli;
    const char *sign = (milli < 0) ? "-" : "";

    if (index != 0U) {
        append("%s[%u]=%s%lu.%03lu %s\r\n", label, (unsigned)index, sign,
            (unsigned long)(magnitude / 1000U),
            (unsigned long)(magnitude % 1000U), unit);
    } else {
        append("%s=%s%lu.%03lu %s\r\n", label, sign,
            (unsigned long)(magnitude / 1000U),
            (unsigned long)(magnitude % 1000U), unit);
    }
}

/* Bridge registers are raw hex; decode them against BQ79600 datasheet 7.5. */
static void append_diagnostics(const bq79600_comm_diag_t *diag)
{
    append("diag_bridge_read=%u %s\r\n", (unsigned)diag->read_status,
        NAME_OF(bringup_status_names, (uint32_t)diag->read_status));
    append("diag_device_config=0x%02X\r\n", (unsigned)diag->device_config);
    append("diag_fault summary=0x%02X sys=0x%02X pwr=0x%02X comm1=0x%02X "
        "comm2=0x%02X\r\n", (unsigned)diag->fault_summary,
        (unsigned)diag->fault_sys, (unsigned)diag->fault_pwr,
        (unsigned)diag->fault_comm1, (unsigned)diag->fault_comm2);
    append("diag_debug comm_stat=0x%02X spi_phy=0x%02X spi_frame=0x%02X "
        "comh_phy=0x%02X comh_frame=0x%02X coml_phy=0x%02X "
        "coml_frame=0x%02X\r\n", (unsigned)diag->debug_comm_stat,
        (unsigned)diag->debug_spi_phy, (unsigned)diag->debug_spi_frame,
        (unsigned)diag->debug_comh_phy, (unsigned)diag->debug_comh_frame,
        (unsigned)diag->debug_coml_phy, (unsigned)diag->debug_coml_frame);
    append("diag_dev1_probe=%u %s addr=0x%02X frame=%02X %02X %02X %02X "
        "%02X %02X %02X\r\n", (unsigned)diag->probe_status,
        NAME_OF(bringup_status_names, (uint32_t)diag->probe_status),
        (unsigned)diag->probe_address,
        (unsigned)diag->probe_frame[0], (unsigned)diag->probe_frame[1],
        (unsigned)diag->probe_frame[2], (unsigned)diag->probe_frame[3],
        (unsigned)diag->probe_frame[4], (unsigned)diag->probe_frame[5],
        (unsigned)diag->probe_frame[6]);
}

static void take_snapshot(const app_data_t *data)
{
    taskENTER_CRITICAL();
    snapshot.bringup_status = data->afe_bringup_status;
    snapshot.bringup_failed_step = data->afe_bringup_failed_step;
    snapshot.configured_devices = data->config.segment_count;
    snapshot.verified_devices = data->afe_verified_devices;
    snapshot.bridge_device_config = data->afe_bridge_device_config;
    snapshot.bringup_attempts = data->afe_bringup_attempts;
    snapshot.diag = data->afe_comm_diag;
    snapshot.afe_samples = data->afe_sample_counter;
    snapshot.afe_errors = data->afe_communication_errors;
    snapshot.afe_last_error = data->afe_last_error;
    snapshot.state = data->ams_state;
    snapshot.active_faults = data->active_faults;
    snapshot.latched_faults = data->latched_faults;
    snapshot.soc_permille = data->soc_permille;
    snapshot.measurement = data->last_measurement;
    taskEXIT_CRITICAL();
    snapshot.now_ms = (uint32_t)HAL_GetTick();
}

static void build_report(void)
{
    const ams_measurement_t *measurement = &snapshot.measurement;
    uint16_t index;

    report_length = 0U;
    append("--- bms t=%lu ms ---\r\n", (unsigned long)snapshot.now_ms);
    append("afe_bringup_status=%u %s\r\n", (unsigned)snapshot.bringup_status,
        NAME_OF(bringup_status_names, snapshot.bringup_status));
    append("afe_bringup_failed_step=%u %s\r\n",
        (unsigned)snapshot.bringup_failed_step,
        NAME_OF(bringup_step_names, snapshot.bringup_failed_step));
    append("afe_verified_devices=%u/%u\r\n",
        (unsigned)snapshot.verified_devices,
        (unsigned)snapshot.configured_devices);
    append("afe_bridge_device_config=0x%02X\r\n",
        (unsigned)snapshot.bridge_device_config);
    append("afe_bringup_attempts=%lu\r\n",
        (unsigned long)snapshot.bringup_attempts);
    if (snapshot.bringup_status != (uint8_t)BQ79600_BRINGUP_OK &&
        snapshot.bringup_failed_step >= (uint8_t)BQ79600_STEP_STACK_WAKE) {
        append_diagnostics(&snapshot.diag);
    }
    append("afe_samples=%lu\r\n", (unsigned long)snapshot.afe_samples);
    append("afe_comm_errors=%lu\r\n", (unsigned long)snapshot.afe_errors);
    append("afe_last_error=%u %s\r\n", (unsigned)snapshot.afe_last_error,
        NAME_OF(reader_error_names, snapshot.afe_last_error));
    append("ams_state=%u %s\r\n", (unsigned)snapshot.state,
        NAME_OF(state_names, (uint32_t)snapshot.state));
    append("active_faults=0x%08lX\r\n", (unsigned long)snapshot.active_faults);
    append("latched_faults=0x%08lX\r\n",
        (unsigned long)snapshot.latched_faults);

    if (measurement->status != AMS_SAMPLE_VALID) {
        append("cells=no valid sample\r\n");
        return;
    }
    append("sample_age_ms=%lu\r\n",
        (unsigned long)(snapshot.now_ms - measurement->timestamp_ms));
    for (index = 0U; index < measurement->valid_cell_count &&
        index < AMS_MAX_CELLS; index++) {
        append_milli("cell", (uint16_t)(index + 1U),
            measurement->cell_voltage_mv[index], "V");
    }
    for (index = 0U; index < measurement->valid_gpio_count &&
        index < AMS_MAX_TEMPERATURES; index++) {
        append_milli("gpio", (uint16_t)(index + 1U),
            measurement->gpio_voltage_mv[index], "V");
    }
    append_milli("pack_voltage", 0U, measurement->pack_voltage_mv, "V");
    append_milli("pack_current", 0U, measurement->pack_current_ma, "A");
    append("soc=%u.%u %%\r\n", (unsigned)(snapshot.soc_permille / 10U),
        (unsigned)(snapshot.soc_permille % 10U));
}

static void transmit_report(void)
{
    if (report_length == 0U) {
        return;
    }
    (void)ulTaskNotifyTake(pdTRUE, 0U);
    tx_waiter = xTaskGetCurrentTaskHandle();
    if (HAL_UART_Transmit_IT(&huart1, (const uint8_t *)report,
        (uint16_t)report_length) != HAL_OK) {
        tx_waiter = NULL;
        return;
    }
    /* The buffer must stay untouched until the interrupt-driven transfer
     * finishes, so wait for it (or abort) before building the next one. */
    if (ulTaskNotifyTake(pdTRUE,
        pdMS_TO_TICKS(UART_BROADCAST_TX_TIMEOUT_MS)) == 0U) {
        (void)HAL_UART_AbortTransmit(&huart1);
    }
    tx_waiter = NULL;
}

void uart_broadcast_task(void *argument)
{
    const app_data_t *data = (const app_data_t *)argument;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        take_snapshot(data);
        build_report();
        transmit_report();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(UART_BROADCAST_PERIOD_MS));
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t woken = pdFALSE;

    if (huart == &huart1 && tx_waiter != NULL) {
        vTaskNotifyGiveFromISR(tx_waiter, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

task_entry_t create_uart_broadcast_task(app_data_t *data)
{
    task_entry_t entry = {0};
    const BaseType_t status = xTaskCreate(uart_broadcast_task,
        "UART broadcast", UART_BROADCAST_STACK_SIZE, data,
        UART_BROADCAST_PRIO, &entry.handle);

    configASSERT(status == pdPASS);
    vTaskSuspend(entry.handle);
    entry.name = "uart_broadcast";
    return entry;
}
