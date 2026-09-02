#ifndef DIGITAL_IO_PINS_H
#define DIGITAL_IO_PINS_H

#include "stm32f4xx_hal_gpio.h"

/* STM32F405RG motherboard mapping recovered from the prior Custom_BMS board
 * project. PB0 is treated as a provisional active-high relay/SDC request and
 * remains low while AMS_HARDWARE_OUTPUTS_COMMISSIONED is zero. */
#define AMS_SDC_ENABLE_GPIO_PORT GPIOB
#define AMS_SDC_ENABLE_PIN       GPIO_PIN_0

/* The current board files expose no independent local fault-output pin. */
#define AMS_FAULT_OUTPUT_AVAILABLE 0U

/* BQ79600 SPI bridge control and status signals. */
#define AMS_AFE_CS_GPIO_PORT       GPIOA
#define AMS_AFE_CS_PIN             GPIO_PIN_4
#define AMS_AFE_SPI_READY_GPIO_PORT GPIOA
#define AMS_AFE_SPI_READY_PIN       GPIO_PIN_3
#define AMS_AFE_NFAULT_GPIO_PORT    GPIOA
#define AMS_AFE_NFAULT_PIN          GPIO_PIN_8

#endif /* DIGITAL_IO_PINS_H */
