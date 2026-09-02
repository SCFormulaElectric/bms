#ifndef DIGITAL_IO_PINS_H
#define DIGITAL_IO_PINS_H

#include "stm32f4xx_hal_gpio.h"

/* Provisional mapping inherited from the donor PCB. The active-high SDC
 * output remains low while AMS_HARDWARE_OUTPUTS_COMMISSIONED is zero. */
#define AMS_SDC_ENABLE_GPIO_PORT GPIOA
#define AMS_SDC_ENABLE_PIN       GPIO_PIN_3
#define AMS_FAULT_GPIO_PORT      GPIOA
#define AMS_FAULT_PIN            GPIO_PIN_4

/* Provisional software-NSS mapping. It is not configured as an output and is
 * never toggled while the default AFE profile is uncommissioned. */
#define AMS_AFE_CS_GPIO_PORT     GPIOC
#define AMS_AFE_CS_PIN           GPIO_PIN_6

#endif /* DIGITAL_IO_PINS_H */
