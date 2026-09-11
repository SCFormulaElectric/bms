#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#define CAN_BUS_BITRATE_BPS                  500000U
#define CAN_AMS_TELEMETRY_PERIOD_MS             100U

/* Edit this one value to move the complete BMS namespace. Its low byte must
 * remain 0 so every generated identifier shares the same 0xNXX prefix. */
#define CAN_BMS_BASE_ID                       0x600U

/* Add or remove names from this mask to choose exactly what is transmitted. */
#define CAN_BMS_MSG_HEARTBEAT             (1UL << 0)
#define CAN_BMS_MSG_PACK_STATUS           (1UL << 1)
#define CAN_BMS_MSG_CELL_EXTREMA          (1UL << 2)
#define CAN_BMS_MSG_TEMPERATURES          (1UL << 3)
#define CAN_BMS_MSG_FAULTS                (1UL << 4)
#define CAN_BMS_MSG_OUTPUTS_LIMITS        (1UL << 5)
#define CAN_BMS_MSG_AFE_STATUS            (1UL << 6)
#define CAN_BMS_MSG_CELL_GROUPS           (1UL << 7)
#define CAN_BMS_MSG_TEMP_GROUPS           (1UL << 8)
#define CAN_BMS_MSG_SUMMARY_PRIMARY       (1UL << 9)
#define CAN_BMS_MSG_SUMMARY_SECONDARY    (1UL << 10)

/* Default: only the two five-second summary frames. An empty mask disables
 * all BMS telemetry without changing the generic CAN transmitter task. */
#define CAN_BMS_ENABLED_MESSAGES \
    (CAN_BMS_MSG_SUMMARY_PRIMARY | CAN_BMS_MSG_SUMMARY_SECONDARY)

#define CAN_BMS_MSG_ALL \
    (CAN_BMS_MSG_HEARTBEAT | CAN_BMS_MSG_PACK_STATUS | \
     CAN_BMS_MSG_CELL_EXTREMA | CAN_BMS_MSG_TEMPERATURES | \
     CAN_BMS_MSG_FAULTS | CAN_BMS_MSG_OUTPUTS_LIMITS | \
     CAN_BMS_MSG_AFE_STATUS | CAN_BMS_MSG_CELL_GROUPS | \
     CAN_BMS_MSG_TEMP_GROUPS | CAN_BMS_MSG_SUMMARY_PRIMARY | \
     CAN_BMS_MSG_SUMMARY_SECONDARY)

#define CAN_BMS_FAST_MESSAGES \
    (CAN_BMS_MSG_HEARTBEAT | CAN_BMS_MSG_PACK_STATUS | \
     CAN_BMS_MSG_CELL_EXTREMA | CAN_BMS_MSG_TEMPERATURES | \
     CAN_BMS_MSG_FAULTS | CAN_BMS_MSG_OUTPUTS_LIMITS | \
     CAN_BMS_MSG_AFE_STATUS | CAN_BMS_MSG_CELL_GROUPS | \
     CAN_BMS_MSG_TEMP_GROUPS)
#define CAN_BMS_FAST_TELEMETRY_ENABLED \
    (((CAN_BMS_ENABLED_MESSAGES) & CAN_BMS_FAST_MESSAGES) != 0U)

/* Subitem offsets within the selected 0xNXX namespace. */
#define CAN_BMS_OFFSET_HEARTBEAT               0x00U
#define CAN_BMS_OFFSET_PACK_STATUS             0x01U
#define CAN_BMS_OFFSET_CELL_EXTREMA            0x02U
#define CAN_BMS_OFFSET_TEMPERATURES            0x03U
#define CAN_BMS_OFFSET_FAULTS                  0x04U
#define CAN_BMS_OFFSET_OUTPUTS_LIMITS          0x05U
#define CAN_BMS_OFFSET_AFE_STATUS              0x06U
#define CAN_BMS_OFFSET_CELL_GROUP_BASE         0x10U
#define CAN_BMS_OFFSET_TEMP_GROUP_BASE         0x40U
#define CAN_BMS_OFFSET_SUMMARY_PRIMARY         0xF0U
#define CAN_BMS_OFFSET_SUMMARY_SECONDARY       0xF1U

#define CAN_BMS_ID(offset) \
    ((CAN_BMS_BASE_ID) + (offset))
#define CAN_BMS_MESSAGE_ENABLED(message) \
    (((CAN_BMS_ENABLED_MESSAGES) & (message)) != 0U)

#define CAN_ID_AMS_HEARTBEAT \
    CAN_BMS_ID(CAN_BMS_OFFSET_HEARTBEAT)
#define CAN_ID_AMS_PACK_STATUS \
    CAN_BMS_ID(CAN_BMS_OFFSET_PACK_STATUS)
#define CAN_ID_AMS_CELL_EXTREMA \
    CAN_BMS_ID(CAN_BMS_OFFSET_CELL_EXTREMA)
#define CAN_ID_AMS_TEMPERATURES \
    CAN_BMS_ID(CAN_BMS_OFFSET_TEMPERATURES)
#define CAN_ID_AMS_FAULTS \
    CAN_BMS_ID(CAN_BMS_OFFSET_FAULTS)
#define CAN_ID_AMS_OUTPUTS_LIMITS \
    CAN_BMS_ID(CAN_BMS_OFFSET_OUTPUTS_LIMITS)
#define CAN_ID_AMS_AFE_STATUS \
    CAN_BMS_ID(CAN_BMS_OFFSET_AFE_STATUS)
#define CAN_ID_AMS_CELL_GROUP_BASE \
    CAN_BMS_ID(CAN_BMS_OFFSET_CELL_GROUP_BASE)
#define CAN_ID_AMS_TEMP_GROUP_BASE \
    CAN_BMS_ID(CAN_BMS_OFFSET_TEMP_GROUP_BASE)
#define CAN_ID_AMS_SUMMARY_PRIMARY \
    CAN_BMS_ID(CAN_BMS_OFFSET_SUMMARY_PRIMARY)
#define CAN_ID_AMS_SUMMARY_SECONDARY \
    CAN_BMS_ID(CAN_BMS_OFFSET_SUMMARY_SECONDARY)

#if ((CAN_BMS_BASE_ID & 0x0FFU) != 0U)
#error "CAN_BMS_BASE_ID must be an aligned namespace such as 0x600"
#endif
#if (CAN_BMS_BASE_ID > 0x700U)
#error "CAN_BMS_BASE_ID and its offsets must fit an 11-bit CAN identifier"
#endif
#if ((CAN_BMS_ENABLED_MESSAGES & ~CAN_BMS_MSG_ALL) != 0U)
#error "CAN_BMS_ENABLED_MESSAGES contains an unknown message bit"
#endif

#endif /* CAN_PROTOCOL_H */
