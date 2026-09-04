#ifndef DIGITAL_IO_PINS_H
#define DIGITAL_IO_PINS_H

#include "stm32f4xx_hal_gpio.h"

/* Confirmed Custom_BMS motherboard mapping. The three 12 V control channels
 * use AO3400A low-side stages: reset keeps each MOSFET off; set pulls the
 * external active-low control net low. */
#define AMS_STATUS_LED_GPIO_PORT        GPIOB
#define AMS_STATUS_LED_PIN              GPIO_PIN_0
#define AMS_CP_CONTROL_GPIO_PORT        GPIOB
#define AMS_CP_CONTROL_PIN              GPIO_PIN_1
#define AMS_DISCHARGE_ENABLE_GPIO_PORT  GPIOB
#define AMS_DISCHARGE_ENABLE_PIN        GPIO_PIN_3
#define AMS_CHARGE_ENABLE_GPIO_PORT     GPIOB
#define AMS_CHARGE_ENABLE_PIN           GPIO_PIN_4
#define AMS_CHARGE_ON_GPIO_PORT         GPIOB
#define AMS_CHARGE_ON_PIN               GPIO_PIN_5
#define AMS_FAN_ENABLE_GPIO_PORT        GPIOB
#define AMS_FAN_ENABLE_PIN              GPIO_PIN_6
#define AMS_PROXIMITY_GPIO_PORT         GPIOA
#define AMS_PROXIMITY_PIN               GPIO_PIN_15
#define AMS_CP_DETECT_GPIO_PORT         GPIOC
#define AMS_CP_DETECT_PIN               GPIO_PIN_10
#define AMS_OUTPUT_DEASSERTED           GPIO_PIN_RESET
#define AMS_OUTPUT_ASSERTED             GPIO_PIN_SET

#define AMS_FAULT_OUTPUT_AVAILABLE      0U

#define AMS_AFE_CS_GPIO_PORT            GPIOA
#define AMS_AFE_CS_PIN                  GPIO_PIN_4
#define AMS_AFE_SPI_READY_GPIO_PORT     GPIOA
#define AMS_AFE_SPI_READY_PIN           GPIO_PIN_3
#define AMS_AFE_NFAULT_GPIO_PORT        GPIOA
#define AMS_AFE_NFAULT_PIN              GPIO_PIN_8
#define AMS_AFE_MOSI_GPIO_PORT          GPIOA
#define AMS_AFE_MOSI_PIN                GPIO_PIN_7
#define AMS_AFE_MOSI_ALTERNATE          GPIO_AF5_SPI1

#endif /* DIGITAL_IO_PINS_H */
