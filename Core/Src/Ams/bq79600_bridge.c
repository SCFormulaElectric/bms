#include "Ams/bq79600_bridge.h"

#include <stddef.h>
#include <string.h>

#include "Ams/ams_config.h"
#include "Ams/bq796xx_protocol.h"
#include "Peripherals/digital_pins.h"

#define BQ79600_REG_DIR0_ADDR             0x0306U
#define BQ79600_REG_COMM_CTRL             0x0308U
#define BQ79600_REG_CONTROL1              0x0309U
#define BQ79600_REG_OTP_ECC_DATAIN1       0x0343U
#define BQ79600_REG_DEVICE_CONFIG         0x2001U
#define BQ79600_CONTROL1_SEND_WAKE        0x20U
#define BQ79600_CONTROL1_AUTO_ADDRESS     0x01U
#define BQ79600_COMM_STACK                0x02U
#define BQ79600_COMM_STACK_TOP            0x03U
#define BQ79600_EXPECTED_DEVICE_CONFIG    0x14U

#define BQ79600_WAKE_PING_COUNT              2U
#define BQ79600_NCS_SETUP_US                 2U
#define BQ79600_WAKE_LOW_US               2750U
#define BQ79600_NCS_HOLD_US                  2U
#define BQ79600_INTER_PING_US                 2U
#define BQ79600_ACTIVE_WAIT_US             3500U
#define BQ79600_STACK_WAKE_PER_DEVICE_US   11600U
#define BQ79600_SPI_FRAME_GUARD_US             1U
#define BQ79600_WRITE_RDY_TIMEOUT_US         250U
#define BQ79600_SPI_HAL_TIMEOUT_MS              5U
#define BQ79600_RESPONSE_BYTES_ONE_REGISTER     7U
#define BQ79600_MAX_RESPONSE_BYTES \
    (AMS_MAX_SEGMENTS * BQ79600_RESPONSE_BYTES_ONE_REGISTER)

static void delay_us(uint32_t microseconds)
{
    const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    const uint32_t start = DWT->CYCCNT;
    const uint32_t cycles = cycles_per_us * microseconds;

    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

static uint8_t enable_cycle_counter(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    __ISB();
    return ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U) ? 1U : 0U;
}

static uint8_t wait_ready(uint32_t timeout_us)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t timeout_cycles =
        (SystemCoreClock / 1000000U) * timeout_us;

    while (HAL_GPIO_ReadPin(AMS_AFE_SPI_READY_GPIO_PORT,
        AMS_AFE_SPI_READY_PIN) != GPIO_PIN_SET) {
        if ((uint32_t)(DWT->CYCCNT - start) >= timeout_cycles) {
            return 0U;
        }
    }
    return 1U;
}

static void configure_mosi_gpio(void)
{
    GPIO_InitTypeDef gpio = {0};

    HAL_GPIO_WritePin(AMS_AFE_MOSI_GPIO_PORT, AMS_AFE_MOSI_PIN,
        GPIO_PIN_SET);
    gpio.Pin = AMS_AFE_MOSI_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(AMS_AFE_MOSI_GPIO_PORT, &gpio);
}

static void restore_mosi_spi(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = AMS_AFE_MOSI_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = AMS_AFE_MOSI_ALTERNATE;
    HAL_GPIO_Init(AMS_AFE_MOSI_GPIO_PORT, &gpio);
}

static bq79600_bringup_status_t gpio_wake(SPI_HandleTypeDef *spi)
{
    uint32_t ping;

    if (enable_cycle_counter() == 0U) {
        return BQ79600_BRINGUP_TIMER_UNAVAILABLE;
    }
    __HAL_SPI_DISABLE(spi);
    HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN, GPIO_PIN_SET);
    configure_mosi_gpio();

    for (ping = 0U; ping < BQ79600_WAKE_PING_COUNT; ping++) {
        const uint32_t interrupt_state = __get_PRIMASK();

        __disable_irq();
        HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN,
            GPIO_PIN_RESET);
        delay_us(BQ79600_NCS_SETUP_US);
        HAL_GPIO_WritePin(AMS_AFE_MOSI_GPIO_PORT, AMS_AFE_MOSI_PIN,
            GPIO_PIN_RESET);
        delay_us(BQ79600_WAKE_LOW_US);
        HAL_GPIO_WritePin(AMS_AFE_MOSI_GPIO_PORT, AMS_AFE_MOSI_PIN,
            GPIO_PIN_SET);
        delay_us(BQ79600_NCS_HOLD_US);
        HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN,
            GPIO_PIN_SET);
        if (interrupt_state == 0U) {
            __enable_irq();
        }
        if ((ping + 1U) < BQ79600_WAKE_PING_COUNT) {
            delay_us(BQ79600_INTER_PING_US);
        }
    }

    restore_mosi_spi();
    delay_us(BQ79600_ACTIVE_WAIT_US);
    return (wait_ready(BQ79600_WRITE_RDY_TIMEOUT_US) != 0U) ?
        BQ79600_BRINGUP_OK : BQ79600_BRINGUP_RDY_TIMEOUT;
}

static bq79600_bringup_status_t comm_clear(SPI_HandleTypeDef *spi)
{
    const uint8_t clear = 0U;
    HAL_StatusTypeDef hal_status;

    HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN, GPIO_PIN_RESET);
    delay_us(BQ79600_SPI_FRAME_GUARD_US);
    hal_status = HAL_SPI_Transmit(spi, (uint8_t *)&clear, 1U,
        BQ79600_SPI_HAL_TIMEOUT_MS);
    delay_us(BQ79600_SPI_FRAME_GUARD_US);
    HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN, GPIO_PIN_SET);
    delay_us(BQ79600_SPI_FRAME_GUARD_US);
    return (hal_status == HAL_OK) ? BQ79600_BRINGUP_OK :
        BQ79600_BRINGUP_SPI_ERROR;
}

static bq79600_bringup_status_t transmit_command(SPI_HandleTypeDef *spi,
    uint8_t *frame, uint16_t length)
{
    HAL_StatusTypeDef hal_status;

    if (wait_ready(BQ79600_WRITE_RDY_TIMEOUT_US) == 0U) {
        if (comm_clear(spi) != BQ79600_BRINGUP_OK ||
            wait_ready(BQ79600_WRITE_RDY_TIMEOUT_US) == 0U) {
            return BQ79600_BRINGUP_RDY_TIMEOUT;
        }
    }
    HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN, GPIO_PIN_RESET);
    delay_us(BQ79600_SPI_FRAME_GUARD_US);
    hal_status = HAL_SPI_Transmit(spi, frame, length,
        BQ79600_SPI_HAL_TIMEOUT_MS);
    delay_us(BQ79600_SPI_FRAME_GUARD_US);
    HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN, GPIO_PIN_SET);
    delay_us(BQ79600_SPI_FRAME_GUARD_US);
    return (hal_status == HAL_OK) ? BQ79600_BRINGUP_OK :
        BQ79600_BRINGUP_SPI_ERROR;
}

static bq79600_bringup_status_t write_register(SPI_HandleTypeDef *spi,
    bq796xx_address_mode_t mode, uint8_t device, uint16_t address,
    uint8_t value)
{
    uint8_t frame[7];
    const uint16_t length = bq796xx_build_write(mode, device, address,
        &value, 1U, frame, sizeof(frame));

    if (length == 0U) {
        return BQ79600_BRINGUP_INVALID_ARGUMENT;
    }
    return transmit_command(spi, frame, length);
}

static uint32_t read_timeout_us(uint8_t devices, uint16_t total_bytes)
{
    /* TI tWAIT_READ_MAX plus 100 us host-side margin. */
    return 200U + (uint32_t)total_bytes * 10U +
        ((devices > 0U) ? (uint32_t)(devices - 1U) * 6U : 0U);
}

static bq79600_bringup_status_t receive_response(SPI_HandleTypeDef *spi,
    uint8_t devices, uint8_t *response, uint16_t response_bytes)
{
    uint8_t dummy[BQ79600_MAX_RESPONSE_BYTES];
    HAL_StatusTypeDef hal_status;

    if (response_bytes == 0U ||
        response_bytes > BQ79600_MAX_RESPONSE_BYTES) {
        return BQ79600_BRINGUP_INVALID_ARGUMENT;
    }
    if (wait_ready(read_timeout_us(devices, response_bytes)) == 0U) {
        (void)comm_clear(spi);
        return BQ79600_BRINGUP_RDY_TIMEOUT;
    }
    memset(dummy, 0xFF, response_bytes);
    HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN, GPIO_PIN_RESET);
    delay_us(BQ79600_SPI_FRAME_GUARD_US);
    hal_status = HAL_SPI_TransmitReceive(spi, dummy, response,
        response_bytes, BQ79600_SPI_HAL_TIMEOUT_MS);
    delay_us(BQ79600_SPI_FRAME_GUARD_US);
    HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN, GPIO_PIN_SET);
    delay_us(BQ79600_SPI_FRAME_GUARD_US);
    if (hal_status != HAL_OK) {
        return BQ79600_BRINGUP_SPI_ERROR;
    }
    return BQ79600_BRINGUP_OK;
}

static bq79600_bringup_status_t read_registers(SPI_HandleTypeDef *spi,
    bq796xx_address_mode_t mode, uint8_t device, uint16_t address,
    uint8_t device_count, uint8_t *response)
{
    uint8_t command[7];
    uint16_t command_length;
    uint16_t response_bytes;
    bq79600_bringup_status_t status;

    command_length = bq796xx_build_read(mode, device, address, 1U,
        command, sizeof(command));
    if (command_length == 0U || device_count == 0U) {
        return BQ79600_BRINGUP_INVALID_ARGUMENT;
    }
    status = transmit_command(spi, command, command_length);
    if (status != BQ79600_BRINGUP_OK) {
        return status;
    }
    response_bytes = (uint16_t)device_count *
        BQ79600_RESPONSE_BYTES_ONE_REGISTER;
    return receive_response(spi, device_count, response, response_bytes);
}

static bq79600_bringup_status_t validate_response(const uint8_t *frame,
    uint8_t expected_device, uint16_t expected_register,
    uint8_t *value)
{
    uint8_t device;
    uint16_t address;
    uint8_t data;
    bq796xx_response_status_t status;

    status = bq796xx_parse_response(frame,
        BQ79600_RESPONSE_BYTES_ONE_REGISTER, &device, &address, &data, 1U);
    if (status == BQ796XX_RESPONSE_BAD_CRC) {
        return BQ79600_BRINGUP_RESPONSE_CRC;
    }
    if (status != BQ796XX_RESPONSE_OK || device != expected_device ||
        address != expected_register) {
        return BQ79600_BRINGUP_RESPONSE_FORMAT;
    }
    *value = data;
    return BQ79600_BRINGUP_OK;
}

static void set_failure(bq79600_bringup_result_t *result,
    bq79600_bringup_status_t status, bq79600_bringup_step_t step)
{
    result->status = status;
    result->failed_step = step;
}

bq79600_bringup_status_t bq79600_stack_bringup(
    SPI_HandleTypeDef *spi, uint8_t stack_devices,
    bq79600_bringup_result_t *result)
{
    bq79600_bringup_result_t local_result = {0};
    bq79600_bringup_status_t status;
    uint8_t index;
    uint8_t response[BQ79600_MAX_RESPONSE_BYTES];
    uint8_t seen[AMS_MAX_SEGMENTS + 1U] = {0};

    if (result == NULL) {
        result = &local_result;
    }
    memset(result, 0, sizeof(*result));
    if (spi == NULL || spi->Instance != SPI1 ||
        HAL_SPI_GetState(spi) != HAL_SPI_STATE_READY ||
        stack_devices == 0U || stack_devices > AMS_MAX_SEGMENTS) {
        set_failure(result, BQ79600_BRINGUP_INVALID_ARGUMENT,
            BQ79600_STEP_NONE);
        return result->status;
    }
    result->configured_stack_devices = stack_devices;

    status = gpio_wake(spi);
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_GPIO_WAKE);
        return status;
    }

    status = write_register(spi, BQ796XX_SINGLE, 0U,
        BQ79600_REG_CONTROL1, BQ79600_CONTROL1_SEND_WAKE);
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_STACK_WAKE);
        return status;
    }
    delay_us((uint32_t)stack_devices *
        BQ79600_STACK_WAKE_PER_DEVICE_US);

    for (index = 0U; index < 8U; index++) {
        status = write_register(spi, BQ796XX_STACK, 0U,
            (uint16_t)(BQ79600_REG_OTP_ECC_DATAIN1 + index), 0U);
        if (status != BQ79600_BRINGUP_OK) {
            set_failure(result, status, BQ79600_STEP_WRITE_DLL_SYNC);
            return status;
        }
    }
    status = write_register(spi, BQ796XX_BROADCAST, 0U,
        BQ79600_REG_CONTROL1, BQ79600_CONTROL1_AUTO_ADDRESS);
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_ENABLE_AUTO_ADDRESS);
        return status;
    }
    for (index = 0U; index <= stack_devices; index++) {
        status = write_register(spi, BQ796XX_BROADCAST, 0U,
            BQ79600_REG_DIR0_ADDR, index);
        if (status != BQ79600_BRINGUP_OK) {
            set_failure(result, status, BQ79600_STEP_ASSIGN_ADDRESSES);
            return status;
        }
    }
    status = write_register(spi, BQ796XX_BROADCAST, 0U,
        BQ79600_REG_COMM_CTRL, BQ79600_COMM_STACK);
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_CONFIGURE_STACK);
        return status;
    }
    status = write_register(spi, BQ796XX_SINGLE, stack_devices,
        BQ79600_REG_COMM_CTRL, BQ79600_COMM_STACK_TOP);
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_CONFIGURE_TOP);
        return status;
    }

    for (index = 0U; index < 8U; index++) {
        status = read_registers(spi, BQ796XX_STACK, 0U,
            (uint16_t)(BQ79600_REG_OTP_ECC_DATAIN1 + index),
            stack_devices, response);
        if (status != BQ79600_BRINGUP_OK) {
            set_failure(result, status, BQ79600_STEP_READ_DLL_SYNC);
            return status;
        }
        /* These reads exist to synchronize the read-direction DLL. TI notes
         * that their returned data need not be complete, so only drain it. */
    }

    status = read_registers(spi, BQ796XX_STACK, 0U,
        BQ79600_REG_DIR0_ADDR, stack_devices, response);
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_VERIFY_ADDRESSES);
        return status;
    }
    for (index = 0U; index < stack_devices; index++) {
        uint8_t device;
        uint16_t address;
        uint8_t value;
        const uint8_t *frame =
            &response[index * BQ79600_RESPONSE_BYTES_ONE_REGISTER];
        const bq796xx_response_status_t parse_status =
            bq796xx_parse_response(frame,
                BQ79600_RESPONSE_BYTES_ONE_REGISTER, &device, &address,
                &value, 1U);
        if (parse_status == BQ796XX_RESPONSE_BAD_CRC) {
            (void)comm_clear(spi);
            set_failure(result, BQ79600_BRINGUP_RESPONSE_CRC,
                BQ79600_STEP_VERIFY_ADDRESSES);
            return result->status;
        }
        if (parse_status != BQ796XX_RESPONSE_OK ||
            address != BQ79600_REG_DIR0_ADDR || device == 0U ||
            device > stack_devices || value != device ||
            seen[device] != 0U) {
            set_failure(result, BQ79600_BRINGUP_ADDRESS_MISMATCH,
                BQ79600_STEP_VERIFY_ADDRESSES);
            return result->status;
        }
        seen[device] = 1U;
        result->verified_stack_devices++;
    }

    status = read_registers(spi, BQ796XX_SINGLE, 0U,
        BQ79600_REG_DEVICE_CONFIG, 1U, response);
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_VERIFY_BRIDGE);
        return status;
    }
    status = validate_response(response, 0U,
        BQ79600_REG_DEVICE_CONFIG, &result->bridge_device_config);
    if (status != BQ79600_BRINGUP_OK) {
        (void)comm_clear(spi);
        set_failure(result, status, BQ79600_STEP_VERIFY_BRIDGE);
        return status;
    }
    if (result->bridge_device_config != BQ79600_EXPECTED_DEVICE_CONFIG) {
        set_failure(result, BQ79600_BRINGUP_DEVICE_CONFIG_MISMATCH,
            BQ79600_STEP_VERIFY_BRIDGE);
        return result->status;
    }

    result->status = BQ79600_BRINGUP_OK;
    result->failed_step = BQ79600_STEP_NONE;
    return BQ79600_BRINGUP_OK;
}
