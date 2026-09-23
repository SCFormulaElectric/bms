#include <assert.h>

#include "Peripherals/can_protocol.h"

int main(void)
{
    uint8_t clear_request[BMS_CAN_FRAME_DLC] = {
        BMS_CAN_PROTOCOL_VERSION, BMS_CAN_CMD_CLEAR_FAULTS, 0x2AU,
        0U, 0U, 0U, 0U, 0U
    };

    assert((CAN_BMS_BASE_ID & 0x0FFU) == 0U);
    assert(BMS_CAN_COMMAND_ID == 0x5E0U);
    assert(BMS_CAN_CONFIG_RESPONSE_ID == 0x6E0U);
    assert(BMS_CAN_SUMMARY_BASE_ID == 0x6F0U);
    assert(BMS_CAN_SUMMARY_MAX_FRAMES == 16U);
    assert(BMS_CAN_CMD_CLEAR_FAULTS == 0x03U);
    assert(bms_can_validate_clear_faults_request(clear_request) ==
        BMS_CAN_STATUS_OK);
    clear_request[0] = 2U;
    assert(bms_can_validate_clear_faults_request(clear_request) ==
        BMS_CAN_STATUS_BAD_VERSION);
    clear_request[0] = BMS_CAN_PROTOCOL_VERSION;
    clear_request[1] = BMS_CAN_CMD_READ_CONFIG;
    assert(bms_can_validate_clear_faults_request(clear_request) ==
        BMS_CAN_STATUS_BAD_OPCODE);
    clear_request[1] = BMS_CAN_CMD_CLEAR_FAULTS;
    clear_request[2] = 0U;
    assert(bms_can_validate_clear_faults_request(clear_request) ==
        BMS_CAN_STATUS_BAD_TRANSACTION);
    clear_request[2] = 0x2AU;
    clear_request[3] = 1U;
    assert(bms_can_validate_clear_faults_request(clear_request) ==
        BMS_CAN_STATUS_BAD_VALUE);
    clear_request[3] = 0U;
    clear_request[7] = 1U;
    assert(bms_can_validate_clear_faults_request(clear_request) ==
        BMS_CAN_STATUS_BAD_VALUE);
    assert(bms_can_validate_clear_faults_request(NULL) ==
        BMS_CAN_STATUS_BAD_VALUE);
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
