#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#define CAN_BUS_BITRATE_BPS          500000U

/* Provisional team-owned identifiers. Freeze these in a DBC before vehicle
 * integration. No incoming CAN frame is safety-authoritative. */
#define CAN_ID_AMS_HEARTBEAT         0x500U
#define CAN_ID_AMS_PACK_STATUS       0x501U
#define CAN_ID_AMS_CELL_EXTREMA      0x502U
#define CAN_ID_AMS_TEMPERATURES      0x503U
#define CAN_ID_AMS_FAULTS            0x504U
#define CAN_ID_AMS_CONTACTORS        0x505U

#endif /* CAN_PROTOCOL_H */
