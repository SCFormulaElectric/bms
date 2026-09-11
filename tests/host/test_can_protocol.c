#include <assert.h>

#include "Peripherals/can_protocol.h"

int main(void)
{
    assert((CAN_BMS_BASE_ID & 0x0FFU) == 0U);
    assert(BMS_CAN_COMMAND_ID == 0x5E0U);
    assert(BMS_CAN_CONFIG_RESPONSE_ID == 0x6E0U);
    assert(BMS_CAN_SUMMARY_BASE_ID == 0x6F0U);
    assert(BMS_CAN_SUMMARY_MAX_FRAMES == 16U);
    assert(CAN_ID_AMS_HEARTBEAT == CAN_BMS_BASE_ID + 0x00U);
    assert(CAN_ID_AMS_PACK_STATUS == CAN_BMS_BASE_ID + 0x01U);
    assert(CAN_ID_AMS_CELL_GROUP_BASE == CAN_BMS_BASE_ID + 0x10U);
    assert(CAN_ID_AMS_TEMP_GROUP_BASE == CAN_BMS_BASE_ID + 0x40U);
    assert(CAN_ID_AMS_SUMMARY_PRIMARY == CAN_BMS_BASE_ID + 0xF0U);
    assert(CAN_ID_AMS_SUMMARY_SECONDARY == CAN_BMS_BASE_ID + 0xF1U);
    assert(CAN_ID_AMS_SUMMARY_SECONDARY <= 0x7FFU);
    assert((CAN_BMS_ENABLED_MESSAGES & ~CAN_BMS_MSG_ALL) == 0U);
    return 0;
}
