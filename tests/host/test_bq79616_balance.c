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

static void test_bal_time(void)
{
    /* BAL_TIME_SEL is the channel minus one in bits 4:1, GO in bit 0. */
    assert(bq79616_bal_time_request(1U) == 0x01U);
    assert(bq79616_bal_time_request(3U) == 0x05U);
    assert(bq79616_bal_time_request(16U) == 0x1FU);
    assert(bq79616_bal_time_request(0U) == 0U);
    assert(bq79616_bal_time_request(17U) == 0U);

    assert(bq79616_bal_time_seconds(0x0CU) == 60U);
    assert(bq79616_bal_time_seconds(0x00U) == 0U);
    assert(bq79616_bal_time_seconds(0x82U) == 600U);
    assert(bq79616_bal_time_seconds(0x7FU) == BQ79616_BAL_TIME_UNKNOWN_S);
    assert(bq79616_bal_time_seconds(0xFFU) == BQ79616_BAL_TIME_UNKNOWN_S);
}

int main(void)
{
    test_cb_ctrl_address();
    test_single_cell_timers();
    test_bal_time();
    return 0;
}
