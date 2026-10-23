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

/* Balance status. CB_COMPLETE1, CB_COMPLETE2 and BAL_TIME are consecutive,
 * so one 3-byte read returns all three. */
#define BQ79616_REG_BAL_CTRL3                0x0330U
#define BQ79616_REG_BAL_STAT                 0x052BU
#define BQ79616_REG_CB_COMPLETE1             0x0556U
#define BQ79616_REG_BAL_TIME                 0x0558U

/* BAL_CTRL3: BAL_TIME_SEL[3:0] in bits 4:1 (0 = CB channel 1) picks the
 * channel whose remaining time BAL_TIME_GO loads into BAL_TIME. */
#define BQ79616_BAL_CTRL3_BAL_TIME_GO        0x01U

#define BQ79616_BAL_STAT_CB_DONE             0x01U
#define BQ79616_BAL_STAT_MB_DONE             0x02U
#define BQ79616_BAL_STAT_ABORTFLT            0x04U
#define BQ79616_BAL_STAT_CB_RUN              0x08U
#define BQ79616_BAL_STAT_MB_RUN              0x10U
#define BQ79616_BAL_STAT_CB_INPAUSE          0x20U
#define BQ79616_BAL_STAT_OT_PAUSE_DET        0x40U
#define BQ79616_BAL_STAT_INVALID_CBCONF      0x80U

/* BAL_TIME: TIME_UNIT (bit 7) selects 5 s or 5 min steps for TIME[6:0].
 * 0x7F/0xFF mean balancing was never started on the selected channel. */
#define BQ79616_BAL_TIME_UNIT_MIN            0x80U
#define BQ79616_BAL_TIME_VALUE_MASK          0x7FU
#define BQ79616_BAL_TIME_INVALID             0x7FU
#define BQ79616_BAL_TIME_UNKNOWN_S           0xFFFFFFFFUL

typedef struct {
    uint8_t bal_stat;          /* BAL_STAT */
    uint16_t cells_done;       /* CB_COMPLETE1:CB_COMPLETE2, bit n-1 = cell n */
    uint8_t bal_time;          /* raw BAL_TIME of the selected channel */
} bq79616_balance_status_t;

/* Address of CB_CELL<cell>_CTRL, or 0 when cell is outside 1-16. */
uint16_t bq79616_cb_ctrl_address(uint8_t cell);

/* BAL_CTRL3 value that loads <cell>'s remaining time into BAL_TIME, or 0
 * when cell is outside 1-16. */
uint8_t bq79616_bal_time_request(uint8_t cell);

/* Remaining balance time in seconds from a raw BAL_TIME value, or
 * BQ79616_BAL_TIME_UNKNOWN_S when the value is the not-started marker.
 * TIME[6:0] is decoded as a count of 5 s (or 5 min) steps, the only reading
 * that covers the 300 s and 600 min timer settings. */
uint32_t bq79616_bal_time_seconds(uint8_t bal_time);

/* Fills CB_CELL16_CTRL..CB_CELL1_CTRL (register order) so only <cell> has a
 * nonzero timer. Returns 0 when cell is outside 1-16 or timer_code is not a
 * nonzero TIME code. */
uint8_t bq79616_single_cell_balance_timers(uint8_t cell, uint8_t timer_code,
    uint8_t timers[BQ79616_CB_CHANNELS]);

#endif /* BQ79616_BALANCE_H */
