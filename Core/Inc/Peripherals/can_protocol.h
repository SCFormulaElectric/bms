#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#define CAN_BUS_BITRATE_BPS          500000U

/* Team-owned telemetry identifiers. Freeze signal definitions in a DBC before
 * vehicle integration. No incoming CAN frame is safety-authoritative. */
#define CAN_ID_AMS_HEARTBEAT         0x500U
#define CAN_ID_AMS_PACK_STATUS       0x501U
#define CAN_ID_AMS_CELL_EXTREMA      0x502U
#define CAN_ID_AMS_TEMPERATURES      0x503U
#define CAN_ID_AMS_FAULTS            0x504U
#define CAN_ID_AMS_OUTPUTS_LIMITS    0x505U
#define CAN_ID_AMS_AFE_STATUS        0x506U
#define CAN_ID_AMS_CELL_GROUP_BASE   0x510U
#define CAN_ID_AMS_TEMP_GROUP_BASE   0x580U
#define CAN_AMS_TELEMETRY_PERIOD_MS     100U

#endif /* CAN_PROTOCOL_H */
