#include <assert.h>

#include "Ams/bq79616_balance.h"

static void test_cb_ctrl_address(void)
{
    assert(bq79616_cb_ctrl_address(1U) == 0x0327U);
    assert(bq79616_cb_ctrl_address(12U) == 0x031CU);
    assert(bq79616_cb_ctrl_address(16U) == 0x0318U);
    assert(bq79616_cb_ctrl_address(0U) == 0U);
    assert(bq79616_cb_ctrl_address(17U) == 0U);
}

static void test_single_cell_timers(void)
{
    uint8_t timers[BQ79616_CB_CHANNELS];
    uint8_t index;

    assert(bq79616_single_cell_balance_timers(1U, BQ79616_CB_TIME_60S,
        timers) != 0U);
    for (index = 0U; index < BQ79616_CB_CHANNELS; index++) {
        /* Last entry is CB_CELL1_CTRL. */
        assert(timers[index] == ((index == 15U) ? BQ79616_CB_TIME_60S : 0U));
    }

    assert(bq79616_single_cell_balance_timers(12U, BQ79616_CB_TIME_10S,
        timers) != 0U);
    assert(timers[4] == BQ79616_CB_TIME_10S);
    assert((uint16_t)(BQ79616_REG_CB_CELL16_CTRL + 4U) ==
        bq79616_cb_ctrl_address(12U));
    assert(timers[15] == 0U);

    assert(bq79616_single_cell_balance_timers(0U, BQ79616_CB_TIME_10S,
        timers) == 0U);
    assert(bq79616_single_cell_balance_timers(17U, BQ79616_CB_TIME_10S,
        timers) == 0U);
    assert(bq79616_single_cell_balance_timers(1U, BQ79616_CB_TIME_STOP,
        timers) == 0U);
    assert(bq79616_single_cell_balance_timers(1U, 0x20U, timers) == 0U);
}

int main(void)
{
    test_cb_ctrl_address();
    test_single_cell_timers();
    return 0;
}
