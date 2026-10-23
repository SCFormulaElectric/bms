#ifndef BQ79600_BRIDGE_H
#define BQ79600_BRIDGE_H

#include <stdint.h>
#include "Ams/bq79616_balance.h"
#include "stm32f4xx_hal.h"

typedef enum {
    BQ79600_BRINGUP_OK = 0,
    BQ79600_BRINGUP_INVALID_ARGUMENT,
    BQ79600_BRINGUP_TIMER_UNAVAILABLE,
    BQ79600_BRINGUP_RDY_TIMEOUT,
    BQ79600_BRINGUP_SPI_ERROR,
    BQ79600_BRINGUP_RESPONSE_FORMAT,
    BQ79600_BRINGUP_RESPONSE_CRC,
    BQ79600_BRINGUP_ADDRESS_MISMATCH,
    BQ79600_BRINGUP_DEVICE_CONFIG_MISMATCH
} bq79600_bringup_status_t;

typedef enum {
    BQ79600_STEP_NONE = 0,
    BQ79600_STEP_GPIO_WAKE,
    BQ79600_STEP_STACK_WAKE,
    BQ79600_STEP_WRITE_DLL_SYNC,
    BQ79600_STEP_ENABLE_AUTO_ADDRESS,
    BQ79600_STEP_ASSIGN_ADDRESSES,
    BQ79600_STEP_CONFIGURE_STACK,
    BQ79600_STEP_CONFIGURE_TOP,
    BQ79600_STEP_READ_DLL_SYNC,
    BQ79600_STEP_VERIFY_ADDRESSES,
    BQ79600_STEP_VERIFY_BRIDGE,
    BQ79600_STEP_CONFIGURE_MEASUREMENT,
    BQ79600_STEP_VERIFY_MEASUREMENT,
    BQ79600_STEP_DISABLE_COMM_TIMEOUT,   /* DEBUG builds only */
    BQ79600_STEP_CONFIGURE_BALANCE,      /* bench balance test only */
    BQ79600_STEP_VERIFY_BALANCE
} bq79600_bringup_step_t;

typedef struct {
    bq79600_bringup_status_t status;
    bq79600_bringup_step_t failed_step;
    uint8_t configured_stack_devices;
    uint8_t verified_stack_devices;
    uint8_t bridge_device_config;
} bq79600_bringup_result_t;

/* BQ79600 fault and communication-debug registers (datasheet SLUSDS1A 7.5),
 * read from the bridge itself after a failed bring-up. Responses from the
 * stack arrive on COMH while CONTROL1 [DIR_SEL] = 0. */
typedef struct {
    bq79600_bringup_status_t read_status; /* OK when every read succeeded */
    uint8_t device_config;     /* 0x2001: expected 0x14 */
    uint8_t fault_summary;     /* 0x2100: FAULT_COMM/REG/SYS/PWR summary */
    uint8_t fault_sys;         /* 0x2102 */
    uint8_t fault_pwr;         /* 0x2103 */
    uint8_t fault_comm1;       /* 0x2104 */
    uint8_t fault_comm2;       /* 0x2105: SPI/COML/COMH frame and bit faults */
    uint8_t debug_comm_stat;   /* 0x2300: reset value 0x13 */
    uint8_t debug_spi_phy;     /* 0x2301 */
    uint8_t debug_spi_frame;   /* 0x2302 */
    uint8_t debug_comh_phy;    /* 0x2304 */
    uint8_t debug_comh_frame;  /* 0x2305 */
    uint8_t debug_coml_phy;    /* 0x2306 */
    uint8_t debug_coml_frame;  /* 0x2307 */
    /* Single-device read of DIR0_ADDR from stack device 1, sent after the
     * register reads above. TI forbids broadcast reads through the bridge,
     * so this asks the one device the stack read could not reach. */
    bq79600_bringup_status_t probe_status;
    uint8_t probe_address;     /* DIR0_ADDR value, valid when probe is OK */
    uint8_t probe_frame[7];    /* raw response bytes, for a bad frame */
} bq79600_comm_diag_t;

/* Complete forward-direction BQ79600 + BQ79616 startup and auto-addressing.
 * stack_devices is the number of BQ79616 daughterboards and excludes the
 * bridge at address zero. Call before SPI DMA and the RTOS scheduler start. */
bq79600_bringup_status_t bq79600_stack_bringup(
    SPI_HandleTypeDef *spi, uint8_t stack_devices,
    bq79600_bringup_result_t *result);

/* Configure every BQ79616 for continuous cell conversion with cells_per_device
 * active cells, and GPIO1..gpios_per_device as ADC-only (absolute voltage)
 * inputs. Call after a successful bq79600_stack_bringup and before the RTOS
 * scheduler starts; it busy-waits for the cell low-pass filter to settle.
 * On failure, result->status and result->failed_step are updated. */
bq79600_bringup_status_t bq79600_configure_measurements(
    SPI_HandleTypeDef *spi, uint8_t stack_devices, uint8_t cells_per_device,
    uint8_t gpios_per_device, bq79600_bringup_result_t *result);

/* Read the bridge's DEVICE_CONFIG, fault and comm-debug registers into
 * *diag, stopping at the first failed read (recorded in diag->read_status),
 * then probe stack device 1 with one single-device read (diag->probe_*).
 * Intended after a bring-up failure once the bridge is awake. Blocking; the
 * SPI must not be running DMA transfers. */
bq79600_bringup_status_t bq79600_read_comm_diagnostics(
    SPI_HandleTypeDef *spi, bq79600_comm_diag_t *diag);

/* Bench-only: bleed one cell of stack device <device> through its balance
 * resistor in manual mode until the BQ79616's own timer expires (timer_code
 * is a CB_CELLn_CTRL TIME code). Clears every other channel's timer and reads
 * the selected one back before BAL_GO. Fault stop and OTCB stay disabled, so
 * the timer is the only limit; an MCU reset does not stop it. Call after
 * bq79600_configure_measurements while the AFE reader is idle (blocking). */
bq79600_bringup_status_t bq79600_start_single_cell_balance(
    SPI_HandleTypeDef *spi, uint8_t device, uint8_t cell, uint8_t timer_code,
    bq79600_bringup_result_t *result);

/* Bench-only: read BAL_STAT, CB_COMPLETE1/2 and <cell>'s remaining balance
 * time (BAL_TIME) from stack device <device>. Blocking, about 1 ms; call
 * only while the AFE reader is idle (between samples). */
bq79600_bringup_status_t bq79600_read_balance_status(
    SPI_HandleTypeDef *spi, uint8_t device, uint8_t cell,
    bq79616_balance_status_t *balance);

#endif /* BQ79600_BRIDGE_H */
