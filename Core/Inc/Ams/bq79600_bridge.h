#ifndef BQ79600_BRIDGE_H
#define BQ79600_BRIDGE_H

#include <stdint.h>
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
    BQ79600_STEP_VERIFY_BRIDGE
} bq79600_bringup_step_t;

typedef struct {
    bq79600_bringup_status_t status;
    bq79600_bringup_step_t failed_step;
    uint8_t configured_stack_devices;
    uint8_t verified_stack_devices;
    uint8_t bridge_device_config;
} bq79600_bringup_result_t;

/* Complete forward-direction BQ79600 + BQ79616 startup and auto-addressing.
 * stack_devices is the number of BQ79616 daughterboards and excludes the
 * bridge at address zero. Call before SPI DMA and the RTOS scheduler start. */
bq79600_bringup_status_t bq79600_stack_bringup(
    SPI_HandleTypeDef *spi, uint8_t stack_devices,
    bq79600_bringup_result_t *result);

#endif /* BQ79600_BRIDGE_H */
