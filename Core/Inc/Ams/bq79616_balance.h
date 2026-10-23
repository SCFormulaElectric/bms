#ifndef BQ79616_BALANCE_H
#define BQ79616_BALANCE_H

#include <stdint.h>

/* BQ79616-Q1 cell-balancing registers. CB_CELL16_CTRL is the lowest address
 * and CB_CELL1_CTRL the highest, the same order as the VCELL block. */
#define BQ79616_REG_CB_CELL16_CTRL           0x0318U
#define BQ79616_REG_CB_CELL1_CTRL            0x0327U
#define BQ79616_REG_BAL_CTRL2                0x032FU

#define BQ79616_CB_CHANNELS                    16U

/* CB_CELLn_CTRL TIME[4:0]: 0 stops the channel, 0x01-0x1F select
 * 10 s to 600 min. */
#define BQ79616_CB_TIME_STOP                 0x00U
#define BQ79616_CB_TIME_10S                  0x01U
#define BQ79616_CB_TIME_30S                  0x02U
#define BQ79616_CB_TIME_60S                  0x03U
#define BQ79616_CB_TIME_300S                 0x04U
#define BQ79616_CB_TIME_MAX                  0x1FU

/* BAL_CTRL2 fields are sampled when BAL_GO is written; BAL_GO self-clears.
 * BAL_ACT[3:2] = 0 leaves the device active when balancing completes. */
#define BQ79616_BAL_CTRL2_AUTO_BAL           0x01U
#define BQ79616_BAL_CTRL2_BAL_GO             0x02U
#define BQ79616_BAL_CTRL2_OTCB_EN            0x10U
#define BQ79616_BAL_CTRL2_FLTSTOP_EN         0x20U
#define BQ79616_BAL_CTRL2_CB_PAUSE           0x40U

/* Address of CB_CELL<cell>_CTRL, or 0 when cell is outside 1-16. */
uint16_t bq79616_cb_ctrl_address(uint8_t cell);

/* Fills CB_CELL16_CTRL..CB_CELL1_CTRL (register order) so only <cell> has a
 * nonzero timer. Returns 0 when cell is outside 1-16 or timer_code is not a
 * nonzero TIME code. */
uint8_t bq79616_single_cell_balance_timers(uint8_t cell, uint8_t timer_code,
    uint8_t timers[BQ79616_CB_CHANNELS]);

#endif /* BQ79616_BALANCE_H */
