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
