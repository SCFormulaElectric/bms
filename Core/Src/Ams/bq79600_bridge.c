#include "Ams/bq79600_bridge.h"

#include <stddef.h>
#include <string.h>

#include "Ams/ams_config.h"
#include "Ams/bq79616_balance.h"
#include "Ams/bq79616_measurement.h"
#include "Ams/bq796xx_protocol.h"
#include "Peripherals/digital_pins.h"

#define BQ79600_REG_DIR0_ADDR             0x0306U
#define BQ79600_REG_COMM_CTRL             0x0308U
#define BQ79600_REG_CONTROL1              0x0309U
#define BQ79600_REG_OTP_ECC_DATAIN1       0x0343U
#define BQ79600_REG_DEVICE_CONFIG         0x2001U
#define BQ79600_REG_COMM_TIMEOUT          0x2005U
#define BQ79600_REG_FAULT_SUMMARY         0x2100U
#define BQ79600_REG_FAULT_SYS             0x2102U
#define BQ79600_REG_FAULT_PWR             0x2103U
#define BQ79600_REG_FAULT_COMM1           0x2104U
#define BQ79600_REG_FAULT_COMM2           0x2105U
#define BQ79600_REG_DEBUG_COMM_STAT       0x2300U
#define BQ79600_REG_DEBUG_SPI_PHY         0x2301U
#define BQ79600_REG_DEBUG_SPI_FRAME       0x2302U
#define BQ79600_REG_DEBUG_COMH_PHY        0x2304U
#define BQ79600_REG_DEBUG_COMH_FRAME      0x2305U
#define BQ79600_REG_DEBUG_COML_PHY        0x2306U
#define BQ79600_REG_DEBUG_COML_FRAME      0x2307U
#define BQ79600_CONTROL1_SEND_WAKE        0x20U
#define BQ79600_CONTROL1_AUTO_ADDRESS     0x01U
#define BQ79600_COMM_STACK                0x02U
#define BQ79600_COMM_STACK_TOP            0x03U
#define BQ79600_EXPECTED_DEVICE_CONFIG    0x14U

#define BQ79600_WAKE_PING_COUNT              2U
#define BQ79600_NCS_SETUP_US                 2U
#define BQ79600_WAKE_LOW_US               2750U
#define BQ79600_NCS_HOLD_US                  2U
/* tSU(WAKE_SHUT): the second ping must follow the first by the maximum
 * SHUTDOWN-to-ACTIVE startup time, not land while the bridge is booting. */
#define BQ79600_INTER_PING_US              3500U
#define BQ79600_ACTIVE_WAIT_US             3500U
#define BQ79600_STACK_WAKE_PER_DEVICE_US   11600U
#define BQ79600_SPI_FRAME_GUARD_US             1U
#define BQ79600_WRITE_RDY_TIMEOUT_US         250U
#define BQ79600_SPI_HAL_TIMEOUT_MS              5U
#define BQ79600_RESPONSE_BYTES_ONE_REGISTER     7U
/* Largest response: a full cell block from every segment. */
#define BQ79600_MAX_RESPONSE_BYTES \
    (AMS_MAX_SEGMENTS * (BQ79616_RESPONSE_OVERHEAD_BYTES + \
        2U * AMS_MAX_CELLS_PER_SEGMENT))
/* The BQ79600 SPI response buffer is drained in blocks of at most 128 bytes,
 * waiting for SPI_RDY before each block. */
#define BQ79600_SPI_READ_CHUNK_BYTES          128U
#define BQ79600_TSREF_SETTLE_US              2000U

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

static bq79600_bringup_status_t write_registers(SPI_HandleTypeDef *spi,
    bq796xx_address_mode_t mode, uint8_t device, uint16_t address,
    const uint8_t *values, uint8_t count)
{
    uint8_t frame[14];
    const uint16_t length = bq796xx_build_write(mode, device, address,
        values, count, frame, sizeof(frame));

    if (length == 0U) {
        return BQ79600_BRINGUP_INVALID_ARGUMENT;
    }
    return transmit_command(spi, frame, length);
}

static bq79600_bringup_status_t write_register(SPI_HandleTypeDef *spi,
    bq796xx_address_mode_t mode, uint8_t device, uint16_t address,
    uint8_t value)
{
    return write_registers(spi, mode, device, address, &value, 1U);
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
    static uint8_t dummy[BQ79600_SPI_READ_CHUNK_BYTES];
    HAL_StatusTypeDef hal_status;
    uint16_t offset = 0U;

    if (response_bytes == 0U ||
        response_bytes > BQ79600_MAX_RESPONSE_BYTES) {
        return BQ79600_BRINGUP_INVALID_ARGUMENT;
    }
    memset(dummy, 0xFF, sizeof(dummy));
    while (offset < response_bytes) {
        uint16_t chunk = (uint16_t)(response_bytes - offset);

        if (chunk > BQ79600_SPI_READ_CHUNK_BYTES) {
            chunk = BQ79600_SPI_READ_CHUNK_BYTES;
        }
        if (wait_ready(read_timeout_us(devices, response_bytes)) == 0U) {
            (void)comm_clear(spi);
            return BQ79600_BRINGUP_RDY_TIMEOUT;
        }
        HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN,
            GPIO_PIN_RESET);
        delay_us(BQ79600_SPI_FRAME_GUARD_US);
        hal_status = HAL_SPI_TransmitReceive(spi, dummy, &response[offset],
            chunk, BQ79600_SPI_HAL_TIMEOUT_MS);
        delay_us(BQ79600_SPI_FRAME_GUARD_US);
        HAL_GPIO_WritePin(AMS_AFE_CS_GPIO_PORT, AMS_AFE_CS_PIN, GPIO_PIN_SET);
        delay_us(BQ79600_SPI_FRAME_GUARD_US);
        if (hal_status != HAL_OK) {
            return BQ79600_BRINGUP_SPI_ERROR;
        }
        offset = (uint16_t)(offset + chunk);
    }
    return BQ79600_BRINGUP_OK;
}

static bq79600_bringup_status_t read_block(SPI_HandleTypeDef *spi,
    bq796xx_address_mode_t mode, uint8_t device, uint16_t address,
    uint8_t data_bytes, uint8_t device_count, uint8_t *response)
{
    uint8_t command[7];
    uint16_t command_length;
    uint16_t response_bytes;
    bq79600_bringup_status_t status;

    command_length = bq796xx_build_read(mode, device, address, data_bytes,
        command, sizeof(command));
    if (command_length == 0U || device_count == 0U) {
        return BQ79600_BRINGUP_INVALID_ARGUMENT;
    }
    status = transmit_command(spi, command, command_length);
    if (status != BQ79600_BRINGUP_OK) {
        return status;
    }
    response_bytes = (uint16_t)device_count *
        (BQ79616_RESPONSE_OVERHEAD_BYTES + (uint16_t)data_bytes);
    return receive_response(spi, device_count, response, response_bytes);
}

static bq79600_bringup_status_t read_registers(SPI_HandleTypeDef *spi,
    bq796xx_address_mode_t mode, uint8_t device, uint16_t address,
    uint8_t device_count, uint8_t *response)
{
    return read_block(spi, mode, device, address, 1U, device_count,
        response);
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

#ifdef DEBUG
    /* By default the bridge drops to SLEEP after one minute without host
     * traffic, which a long breakpoint triggers and nothing re-wakes. */
    status = write_register(spi, BQ796XX_SINGLE, 0U,
        BQ79600_REG_COMM_TIMEOUT, 0U);
    if (status == BQ79600_BRINGUP_OK) {
        status = read_registers(spi, BQ796XX_SINGLE, 0U,
            BQ79600_REG_COMM_TIMEOUT, 1U, response);
    }
    if (status == BQ79600_BRINGUP_OK) {
        uint8_t comm_timeout;

        status = validate_response(response, 0U, BQ79600_REG_COMM_TIMEOUT,
            &comm_timeout);
        if (status != BQ79600_BRINGUP_OK) {
            (void)comm_clear(spi);
        } else if (comm_timeout != 0U) {
            status = BQ79600_BRINGUP_RESPONSE_FORMAT;
        }
    }
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_DISABLE_COMM_TIMEOUT);
        return status;
    }
#endif

    result->status = BQ79600_BRINGUP_OK;
    result->failed_step = BQ79600_STEP_NONE;
    return BQ79600_BRINGUP_OK;
}

static uint8_t measurement_topology_valid(SPI_HandleTypeDef *spi,
    uint8_t stack_devices, uint8_t cells_per_device,
    uint8_t gpios_per_device)
{
    return (spi != NULL && spi->Instance == SPI1 &&
        stack_devices != 0U && stack_devices <= AMS_MAX_SEGMENTS &&
        cells_per_device >= BQ79616_MIN_CELLS &&
        cells_per_device <= AMS_MAX_CELLS_PER_SEGMENT &&
        gpios_per_device <= AMS_MAX_TEMPS_PER_SEGMENT) ? 1U : 0U;
}

bq79600_bringup_status_t bq79600_configure_measurements(
    SPI_HandleTypeDef *spi, uint8_t stack_devices, uint8_t cells_per_device,
    uint8_t gpios_per_device, bq79600_bringup_result_t *result)
{
    bq79600_bringup_result_t local_result = {0};
    bq79600_bringup_status_t status;
    uint8_t gpio_conf[BQ79616_GPIO_CONF_REGISTERS];
    uint8_t response[BQ79600_MAX_RESPONSE_BYTES];
    const uint8_t active_cell = bq79616_active_cell_value(cells_per_device);
    uint8_t index;

    if (result == NULL) {
        result = &local_result;
    }
    if (measurement_topology_valid(spi, stack_devices, cells_per_device,
        gpios_per_device) == 0U ||
        bq79616_gpio_voltage_conf(gpios_per_device, gpio_conf) == 0U) {
        set_failure(result, BQ79600_BRINGUP_INVALID_ARGUMENT,
            BQ79600_STEP_CONFIGURE_MEASUREMENT);
        return result->status;
    }

    status = write_register(spi, BQ796XX_STACK, 0U,
        BQ79616_REG_ACTIVE_CELL, active_cell);
    if (status == BQ79600_BRINGUP_OK) {
        status = write_registers(spi, BQ796XX_STACK, 0U,
            BQ79616_REG_GPIO_CONF1, gpio_conf, sizeof(gpio_conf));
    }
    if (status == BQ79600_BRINGUP_OK) {
        /* TSREF biases the daughterboard thermistor dividers, so the GPIO
         * voltages are meaningful even though they are not converted. */
        status = write_register(spi, BQ796XX_STACK, 0U,
            BQ79616_REG_CONTROL2, BQ79616_CONTROL2_TSREF_EN);
    }
    if (status == BQ79600_BRINGUP_OK) {
        delay_us(BQ79600_TSREF_SETTLE_US);
        status = write_register(spi, BQ796XX_STACK, 0U,
            BQ79616_REG_ADC_CONF1, BQ79616_ADC_CONF1_LPF_VCELL_26HZ);
    }
    if (status == BQ79600_BRINGUP_OK) {
        status = write_register(spi, BQ796XX_STACK, 0U,
            BQ79616_REG_ADC_CTRL1, BQ79616_ADC_CTRL1_CONTINUOUS);
    }
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_CONFIGURE_MEASUREMENT);
        return status;
    }

    status = read_registers(spi, BQ796XX_STACK, 0U,
        BQ79616_REG_ACTIVE_CELL, stack_devices, response);
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_VERIFY_MEASUREMENT);
        return status;
    }
    for (index = 0U; index < stack_devices; index++) {
        uint8_t device;
        uint16_t address;
        uint8_t value;
        const bq796xx_response_status_t parse_status =
            bq796xx_parse_response(
                &response[index * BQ79600_RESPONSE_BYTES_ONE_REGISTER],
                BQ79600_RESPONSE_BYTES_ONE_REGISTER, &device, &address,
                &value, 1U);
        if (parse_status == BQ796XX_RESPONSE_BAD_CRC) {
            (void)comm_clear(spi);
            set_failure(result, BQ79600_BRINGUP_RESPONSE_CRC,
                BQ79600_STEP_VERIFY_MEASUREMENT);
            return result->status;
        }
        if (parse_status != BQ796XX_RESPONSE_OK ||
            address != BQ79616_REG_ACTIVE_CELL || device == 0U ||
            device > stack_devices || value != active_cell) {
            set_failure(result, BQ79600_BRINGUP_RESPONSE_FORMAT,
                BQ79600_STEP_VERIFY_MEASUREMENT);
            return result->status;
        }
    }

    delay_us(BQ79616_LPF_VCELL_26HZ_SETTLE_US + 5U * stack_devices);
    return BQ79600_BRINGUP_OK;
}

bq79600_bringup_status_t bq79600_read_comm_diagnostics(
    SPI_HandleTypeDef *spi, bq79600_comm_diag_t *diag)
{
    static const struct {
        uint16_t address;
        size_t offset;
    } registers[] = {
        {BQ79600_REG_DEVICE_CONFIG,
            offsetof(bq79600_comm_diag_t, device_config)},
        {BQ79600_REG_FAULT_SUMMARY,
            offsetof(bq79600_comm_diag_t, fault_summary)},
        {BQ79600_REG_FAULT_SYS, offsetof(bq79600_comm_diag_t, fault_sys)},
        {BQ79600_REG_FAULT_PWR, offsetof(bq79600_comm_diag_t, fault_pwr)},
        {BQ79600_REG_FAULT_COMM1,
            offsetof(bq79600_comm_diag_t, fault_comm1)},
        {BQ79600_REG_FAULT_COMM2,
            offsetof(bq79600_comm_diag_t, fault_comm2)},
        {BQ79600_REG_DEBUG_COMM_STAT,
            offsetof(bq79600_comm_diag_t, debug_comm_stat)},
        {BQ79600_REG_DEBUG_SPI_PHY,
            offsetof(bq79600_comm_diag_t, debug_spi_phy)},
        {BQ79600_REG_DEBUG_SPI_FRAME,
            offsetof(bq79600_comm_diag_t, debug_spi_frame)},
        {BQ79600_REG_DEBUG_COMH_PHY,
            offsetof(bq79600_comm_diag_t, debug_comh_phy)},
        {BQ79600_REG_DEBUG_COMH_FRAME,
            offsetof(bq79600_comm_diag_t, debug_comh_frame)},
        {BQ79600_REG_DEBUG_COML_PHY,
            offsetof(bq79600_comm_diag_t, debug_coml_phy)},
        {BQ79600_REG_DEBUG_COML_FRAME,
            offsetof(bq79600_comm_diag_t, debug_coml_frame)}
    };
    uint8_t response[BQ79600_RESPONSE_BYTES_ONE_REGISTER];
    bq79600_bringup_status_t status = BQ79600_BRINGUP_OK;
    uint8_t index;

    if (diag == NULL) {
        return BQ79600_BRINGUP_INVALID_ARGUMENT;
    }
    memset(diag, 0, sizeof(*diag));
    if (spi == NULL || spi->Instance != SPI1) {
        diag->read_status = BQ79600_BRINGUP_INVALID_ARGUMENT;
        diag->probe_status = BQ79600_BRINGUP_INVALID_ARGUMENT;
        return diag->read_status;
    }
    for (index = 0U; index < sizeof(registers) / sizeof(registers[0]) &&
        status == BQ79600_BRINGUP_OK; index++) {
        status = read_registers(spi, BQ796XX_SINGLE, 0U,
            registers[index].address, 1U, response);
        if (status == BQ79600_BRINGUP_OK) {
            status = validate_response(response, 0U,
                registers[index].address,
                (uint8_t *)diag + registers[index].offset);
            if (status != BQ79600_BRINGUP_OK) {
                (void)comm_clear(spi);
            }
        }
    }
    diag->read_status = status;

    /* Bridge registers are read first so they reflect the bring-up failure,
     * not this probe. A response that parses but is wrong stays in
     * probe_frame for inspection. */
    diag->probe_status = read_registers(spi, BQ796XX_SINGLE, 1U,
        BQ79600_REG_DIR0_ADDR, 1U, diag->probe_frame);
    if (diag->probe_status == BQ79600_BRINGUP_OK) {
        diag->probe_status = validate_response(diag->probe_frame, 1U,
            BQ79600_REG_DIR0_ADDR, &diag->probe_address);
        if (diag->probe_status != BQ79600_BRINGUP_OK) {
            (void)comm_clear(spi);
        }
    }
    return status;
}

bq79600_bringup_status_t bq79600_start_single_cell_balance(
    SPI_HandleTypeDef *spi, uint8_t device, uint8_t cell, uint8_t timer_code,
    bq79600_bringup_result_t *result)
{
    bq79600_bringup_result_t local_result = {0};
    bq79600_bringup_status_t status;
    uint8_t timers[BQ79616_CB_CHANNELS];
    uint8_t response[BQ79600_RESPONSE_BYTES_ONE_REGISTER];
    const uint16_t timer_address = bq79616_cb_ctrl_address(cell);
    uint8_t value = 0U;

    if (result == NULL) {
        result = &local_result;
    }
    if (spi == NULL || spi->Instance != SPI1 || device == 0U ||
        device > AMS_MAX_SEGMENTS || cell > AMS_MAX_CELLS_PER_SEGMENT ||
        bq79616_single_cell_balance_timers(cell, timer_code, timers) == 0U) {
        set_failure(result, BQ79600_BRINGUP_INVALID_ARGUMENT,
            BQ79600_STEP_CONFIGURE_BALANCE);
        return result->status;
    }

    /* A write frame carries at most eight data bytes. */
    status = write_registers(spi, BQ796XX_SINGLE, device,
        BQ79616_REG_CB_CELL16_CTRL, timers, 8U);
    if (status == BQ79600_BRINGUP_OK) {
        status = write_registers(spi, BQ796XX_SINGLE, device,
            (uint16_t)(BQ79616_REG_CB_CELL16_CTRL + 8U), &timers[8], 8U);
    }
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_CONFIGURE_BALANCE);
        return status;
    }

    status = read_registers(spi, BQ796XX_SINGLE, device, timer_address, 1U,
        response);
    if (status == BQ79600_BRINGUP_OK) {
        status = validate_response(response, device, timer_address, &value);
        if (status != BQ79600_BRINGUP_OK) {
            (void)comm_clear(spi);
        }
    }
    if (status == BQ79600_BRINGUP_OK && value != timer_code) {
        status = BQ79600_BRINGUP_RESPONSE_FORMAT;
    }
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_VERIFY_BALANCE);
        return status;
    }

    /* Manual mode turns on every channel with a nonzero timer: here, one. */
    status = write_register(spi, BQ796XX_SINGLE, device,
        BQ79616_REG_BAL_CTRL2, BQ79616_BAL_CTRL2_BAL_GO);
    if (status != BQ79600_BRINGUP_OK) {
        set_failure(result, status, BQ79600_STEP_CONFIGURE_BALANCE);
    }
    return status;
}

bq79600_bringup_status_t bq79600_read_balance_status(
    SPI_HandleTypeDef *spi, uint8_t device, uint8_t cell,
    bq79616_balance_status_t *balance)
{
    uint8_t response[BQ79616_RESPONSE_OVERHEAD_BYTES + 3U];
    uint8_t data[3];
    uint8_t responder;
    uint16_t address;
    bq796xx_response_status_t parse_status;
    bq79600_bringup_status_t status;
    const uint8_t request = bq79616_bal_time_request(cell);

    if (spi == NULL || spi->Instance != SPI1 || balance == NULL ||
        device == 0U || device > AMS_MAX_SEGMENTS || request == 0U) {
        return BQ79600_BRINGUP_INVALID_ARGUMENT;
    }

    /* Latch the selected channel's remaining time into BAL_TIME. */
    status = write_register(spi, BQ796XX_SINGLE, device,
        BQ79616_REG_BAL_CTRL3, request);
    if (status == BQ79600_BRINGUP_OK) {
        status = read_registers(spi, BQ796XX_SINGLE, device,
            BQ79616_REG_BAL_STAT, 1U, response);
    }
    if (status == BQ79600_BRINGUP_OK) {
        status = validate_response(response, device, BQ79616_REG_BAL_STAT,
            &balance->bal_stat);
        if (status != BQ79600_BRINGUP_OK) {
            (void)comm_clear(spi);
        }
    }
    if (status == BQ79600_BRINGUP_OK) {
        status = read_block(spi, BQ796XX_SINGLE, device,
            BQ79616_REG_CB_COMPLETE1, sizeof(data), 1U, response);
    }
    if (status != BQ79600_BRINGUP_OK) {
        return status;
    }
    parse_status = bq796xx_parse_response(response, sizeof(response),
        &responder, &address, data, sizeof(data));
    if (parse_status != BQ796XX_RESPONSE_OK || responder != device ||
        address != BQ79616_REG_CB_COMPLETE1) {
        (void)comm_clear(spi);
        return (parse_status == BQ796XX_RESPONSE_BAD_CRC) ?
            BQ79600_BRINGUP_RESPONSE_CRC : BQ79600_BRINGUP_RESPONSE_FORMAT;
    }
    balance->cells_done = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    balance->bal_time = data[2];
    return BQ79600_BRINGUP_OK;
}
