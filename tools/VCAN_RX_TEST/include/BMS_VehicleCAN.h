/*
 * BMS_VehicleCAN.h  -  Decoder for the BMS -> vehicle CAN frames.
 *
 *  HAND-OFF FILE. Drop this single header into any ESP32 (or other MCU) project
 *  that receives CAN. It ONLY decodes the two BMS frames into plain structs -
 *  it does NOT touch the CAN peripheral, so it works with any CAN stack (ESP32
 *  TWAI, MCP2515/mcp_can, ...). You keep your own reception loop and pass the
 *  raw (id, data, len) of each received frame to the decode functions below.
 *
 *  Usage:
 *      #include "BMS_VehicleCAN.h"
 *      BmsStatus st;
 *      if (BmsVcan_DecodeStatus(rxId, rxData, rxLen, &st)) {
 *          if (st.state == BMS_STATE_FAULT)      motor_stop();
 *          if (st.flags & BMS_FLAG_OVER_CURRENT) motor_derate();
 *          // st.total_voltage_v, st.current_a, st.sys_soc ...
 *      }
 *      BmsPack pk;
 *      if (BmsVcan_DecodePack(rxId, rxData, rxLen, &pk)) {
 *          // pk.index, pk.voltage_v, pk.soc, pk.temp_c, pk.online ...
 *      }
 *
 *  Protocol (fixed by the BMS master firmware, big-endian / MSB first):
 *
 *  0x200 BMS_STATUS  (DLC 8, every 100 ms)
 *    byte 0-1  total pack voltage   uint16  x0.01 V
 *    byte 2-3  pack current         int16   x0.01 A  (>0 = discharge)
 *                                           -32768   = current sensor lost
 *    byte 4    system SOC           uint8   1 %      0xFF = unknown
 *    byte 5    state                uint8   BmsState (0..4)
 *    byte 6    fault flags          uint8   BMS_FLAG_* bitmask
 *    byte 7    alive counter        uint8   +1 each frame (watch it move!)
 *
 *  0x201 BMS_PACK  (DLC 8, every 100 ms, one battery per frame, round-robin)
 *    byte 0    pack index           uint8   0..count-1
 *    byte 1-2  pack voltage         uint16  x0.01 V
 *    byte 3    pack SOC             uint8   1 %      0xFF = unknown
 *    byte 4    pack temperature     int8    1 C      0x80 = unknown
 *    byte 5    pack fault flags     uint8
 *    byte 6    online               uint8   0 / 1
 *    byte 7    pack count           uint8   how many batteries are monitored
 */
#ifndef BMS_VEHICLE_CAN_H
#define BMS_VEHICLE_CAN_H

#include <stdint.h>
#include <stdbool.h>

/* ---- CAN identifiers ---- */
#define BMS_VCAN_STATUS_ID   0x200u
#define BMS_VCAN_PACK_ID     0x201u

/* ---- invalid / "unknown" markers as sent on the wire ---- */
#define BMS_VCAN_SOC_INVALID_RAW      0xFF
#define BMS_VCAN_TEMP_INVALID_RAW     0x80        /* int8 = -128 */
#define BMS_VCAN_CURRENT_INVALID_RAW  (-32768)    /* int16 min   */

/* ---- fault flag bits (byte 6 of 0x200; byte 5 of 0x201 uses bits 0-2) ---- */
#define BMS_FLAG_OVER_VOLT     0x01
#define BMS_FLAG_UNDER_VOLT    0x02
#define BMS_FLAG_OVER_TEMP     0x04
#define BMS_FLAG_OVER_CURRENT  0x08
#define BMS_FLAG_SLAVE_LOST    0x10
#define BMS_FLAG_RELAY_OPEN    0x20
#define BMS_FLAG_MANUAL_OFF    0x40    /* operator turned it off on the web UI -
                                        * NOT a fault; react differently         */
#define BMS_FLAG_BMS_INTERNAL  0x80

/* ---- pack state (byte 5 of 0x200) ---- */
enum {
    BMS_STATE_INIT      = 0,   /* not all batteries seen yet - data not trusted */
    BMS_STATE_IDLE      = 1,
    BMS_STATE_DISCHARGE = 2,
    BMS_STATE_CHARGE    = 3,
    BMS_STATE_FAULT     = 4    /* relay is open */
};

/* ---- decoded 0x200 ---- */
typedef struct {
    float   total_voltage_v;   /* sum of online batteries                    */
    float   current_a;         /* >0 = discharge; 0 if !current_valid        */
    bool    current_valid;     /* false = BMS lost its current sensor        */
    int16_t sys_soc;           /* 0..100 %, or -1 = unknown                  */
    uint8_t state;             /* one of BMS_STATE_*                         */
    uint8_t flags;             /* BMS_FLAG_* bitmask                         */
    uint8_t alive_counter;     /* stalls if the BMS hangs                    */
} BmsStatus;

/* ---- decoded 0x201 ---- */
typedef struct {
    uint8_t index;             /* which battery this frame describes         */
    float   voltage_v;
    int16_t soc;               /* 0..100 %, or -1 = unknown                  */
    int16_t temp_c;            /* degrees C, or -1000 = unknown              */
    bool    online;            /* false = this battery is not reporting      */
    uint8_t flags;             /* per-battery fault bits (0x01/0x02/0x04)    */
    uint8_t pack_count;        /* total batteries monitored (frame-described)*/
} BmsPack;

/* Decode a 0x200 frame. Returns false (leaves *out untouched) if the id/length
 * do not match, so it is safe to call on every received frame. */
static inline bool BmsVcan_DecodeStatus(uint32_t id, const uint8_t *d,
                                        uint8_t len, BmsStatus *out) {
    if (id != BMS_VCAN_STATUS_ID || len < 8 || d == 0 || out == 0) {
        return false;
    }
    uint16_t v_raw = (uint16_t)(((uint16_t)d[0] << 8) | d[1]);
    int16_t  i_raw = (int16_t) (((uint16_t)d[2] << 8) | d[3]);

    out->total_voltage_v = (float)v_raw * 0.01f;
    out->current_valid   = (i_raw != BMS_VCAN_CURRENT_INVALID_RAW);
    out->current_a       = out->current_valid ? ((float)i_raw * 0.01f) : 0.0f;
    out->sys_soc         = (d[4] == BMS_VCAN_SOC_INVALID_RAW) ? (int16_t)-1
                                                              : (int16_t)d[4];
    out->state           = d[5];
    out->flags           = d[6];
    out->alive_counter   = d[7];
    return true;
}

/* Decode a 0x201 frame. Same contract as above. */
static inline bool BmsVcan_DecodePack(uint32_t id, const uint8_t *d,
                                      uint8_t len, BmsPack *out) {
    if (id != BMS_VCAN_PACK_ID || len < 8 || d == 0 || out == 0) {
        return false;
    }
    uint16_t v_raw = (uint16_t)(((uint16_t)d[1] << 8) | d[2]);

    out->index      = d[0];
    out->online     = (d[6] != 0);
    out->voltage_v  = (float)v_raw * 0.01f;
    out->soc        = (d[3] == BMS_VCAN_SOC_INVALID_RAW)  ? (int16_t)-1
                                                          : (int16_t)d[3];
    out->temp_c     = (d[4] == BMS_VCAN_TEMP_INVALID_RAW) ? (int16_t)-1000
                                                          : (int16_t)(int8_t)d[4];
    out->flags      = d[5];
    out->pack_count = d[7];
    return true;
}

/* Human-readable state name (for logging). */
static inline const char *BmsVcan_StateName(uint8_t state) {
    switch (state) {
        case BMS_STATE_INIT:      return "INIT";
        case BMS_STATE_IDLE:      return "IDLE";
        case BMS_STATE_DISCHARGE: return "DISCHARGE";
        case BMS_STATE_CHARGE:    return "CHARGE";
        case BMS_STATE_FAULT:     return "FAULT";
        default:                  return "?";
    }
}

#endif /* BMS_VEHICLE_CAN_H */
