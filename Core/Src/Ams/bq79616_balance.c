#include "Ams/bq79616_balance.h"

#include <stddef.h>
#include <string.h>

uint16_t bq79616_cb_ctrl_address(uint8_t cell)
{
    if (cell == 0U || cell > BQ79616_CB_CHANNELS) {
        return 0U;
    }
    return (uint16_t)(BQ79616_REG_CB_CELL1_CTRL - (uint16_t)(cell - 1U));
}

uint8_t bq79616_bal_time_request(uint8_t cell)
{
    if (cell == 0U || cell > BQ79616_CB_CHANNELS) {
        return 0U;
    }
    return (uint8_t)(((uint8_t)(cell - 1U) << 1) |
        BQ79616_BAL_CTRL3_BAL_TIME_GO);
}

uint32_t bq79616_bal_time_seconds(uint8_t bal_time)
{
    const uint32_t steps = bal_time & BQ79616_BAL_TIME_VALUE_MASK;

    if (steps == BQ79616_BAL_TIME_INVALID) {
        return BQ79616_BAL_TIME_UNKNOWN_S;
    }
    return ((bal_time & BQ79616_BAL_TIME_UNIT_MIN) != 0U) ?
        steps * 5U * 60U : steps * 5U;
}

uint8_t bq79616_single_cell_balance_timers(uint8_t cell, uint8_t timer_code,
    uint8_t timers[BQ79616_CB_CHANNELS])
{
    if (timers == NULL || cell == 0U || cell > BQ79616_CB_CHANNELS ||
        timer_code == BQ79616_CB_TIME_STOP ||
        timer_code > BQ79616_CB_TIME_MAX) {
        return 0U;
    }
    memset(timers, 0, BQ79616_CB_CHANNELS);
    timers[BQ79616_CB_CHANNELS - cell] = timer_code;
    return 1U;
}
