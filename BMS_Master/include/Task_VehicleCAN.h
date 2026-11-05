/**
 * @file      Task_VehicleCAN.h
 * @author    Do Dang Gia Huy
 * @brief     BMS to vehicle CAN protocol and its MCP2515 transmitter.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Same conventions as the internal bus, deliberately not a second set:
 *     MSB first, int16 two's complement, current > 0 means DISCHARGE.
 *
 * Both frames carry EXACTLY the data the web dashboard shows, so the two
 * outputs can be cross-checked against each other by eye.
 *
 * ---- kVcanStatusId (0x200), DLC 8, every kVcanPeriodMs --------------
 *   byte 0-1  Total voltage of ONLINE packs  uint16  0.01 V
 *   byte 2-3  Pack current                   int16   0.01 A, >0 = discharge
 *                                            VCAN_CURRENT_INVALID if lost
 *   byte 4    System SOC                     uint8   1 %, VCAN_SOC_INVALID
 *   byte 5    State                          uint8   VCAN_State_t
 *   byte 6    Fault flags                    uint8   VCAN_FLAG_* bitmask
 *   byte 7    Alive counter                  uint8   increments, wraps
 *
 * ---- kVcanPackId (0x201), DLC 8, MULTIPLEXED one pack per period ----
 *   byte 0    Pack index (the selector)      uint8   0..count-1
 *   byte 1-2  Pack voltage                   uint16  0.01 V
 *   byte 3    Pack SOC (slave Kalman)        uint8   1 %, VCAN_SOC_INVALID
 *   byte 4    Pack temperature               int8    1 C, VCAN_TEMP_INVALID
 *   byte 5    Pack fault flags               uint8   = byte 6 of its frame
 *   byte 6    Online                         uint8   0 or 1
 *   byte 7    Packs monitored                uint8   System_GetPackCount()
 *
 * Five packs are therefore swept in five periods, 500 ms. Pack voltage and
 * temperature change very slowly, so that is fast enough; a vehicle needing
 * instantaneous figures reads 0x200.
 *
 * Byte 7 of 0x201 makes the frame SELF DESCRIBING: the vehicle learns how
 * many indices to expect instead of hard-coding five, which also covers the
 * planned "set the pack count from the web UI" feature.
 *
 * WHY there are INVALID values: System_MinSoc() returns -1 when a slave is
 * offline, because a missing pack might be the weakest one. Sending a
 * plausible-LOOKING number when there is no data is more dangerous than
 * saying "unknown".
 *
 * WHY there is an alive counter: the receiver checks that it INCREMENTS.
 * Frames still arriving while the counter stands still means the BMS has
 * hung, and a frame timeout does NOT catch that case.
 *
 * Naming note: VCAN_State_t and the VCAN_* constants keep their C spelling
 * instead of the enum class the style guide prefers. Their names AND values
 * are the published protocol, reproduced in the project report, so code and
 * report have to read the same.
 */

#ifndef TASK_VEHICLE_CAN_H_
#define TASK_VEHICLE_CAN_H_

/**********************
 *      INCLUDES
 *********************/

#include <Arduino.h>

#include <mcp_can.h>

#include "System_Data.h"

/**********************
 *      DEFINES
 *********************/

/* Bits 0-2 are taken STRAIGHT from the slave status byte. The slave uses
 * only the low three (0x01 / 0x02 / 0x04), so bit 3 upwards is free for the
 * master to define. */
constexpr uint8_t VCAN_FLAG_OVER_VOLT    = 0x01U;
constexpr uint8_t VCAN_FLAG_UNDER_VOLT   = 0x02U;
constexpr uint8_t VCAN_FLAG_OVER_TEMP    = 0x04U;
constexpr uint8_t VCAN_FLAG_OVER_CURRENT = 0x08U;
constexpr uint8_t VCAN_FLAG_SLAVE_LOST   = 0x10U;
constexpr uint8_t VCAN_FLAG_RELAY_OPEN   = 0x20U;

/** Operator opened the relay from the web page. DIFFERENT from a fault
 * trip: the vehicle reacts to the two differently. */
constexpr uint8_t VCAN_FLAG_MANUAL_OFF = 0x40U;

constexpr uint8_t VCAN_FLAG_BMS_INTERNAL = 0x80U;

constexpr uint8_t VCAN_SOC_INVALID  = 0xFFU;
constexpr int8_t  VCAN_TEMP_INVALID = -128;     /**< int8 minimum */

/** int16 minimum, so bytes 2-3 read 0x80 0x00. Same device as
 * VCAN_TEMP_INVALID: when the current sensor is lost, say "unknown" rather
 * than send 0 A, which is a plausible-LOOKING number. */
constexpr int16_t VCAN_CURRENT_INVALID = -32768;

/** Payload length of both vehicle frames. */
constexpr uint8_t kVcanFrameDlc = 8U;

/**********************
 *      TYPEDEFS
 **********************/

/** Byte 5 of 0x200. Values are protocol and must not be renumbered. */
typedef enum
{
    VCAN_STATE_INIT      = 0,   /**< not every pack has been seen yet, so the
                                 *   data cannot be trusted                 */
    VCAN_STATE_IDLE      = 1,
    VCAN_STATE_DISCHARGE = 2,
    VCAN_STATE_CHARGE    = 3,
    VCAN_STATE_FAULT     = 4    /**< the relay is open                      */
} VCAN_State_t;

/** Owns the MCP2515 and the multiplex position of the 0x201 sweep. */
class VehicleCanManager
{
public:
    /**
     * @brief  Bind the driver to its chip-select pin; touches no hardware.
     * @param[in] csPin  SPI chip select of the MCP2515.
     */
    explicit VehicleCanManager(uint8_t csPin);

    /**
     * @brief  Start SPI and bring the MCP2515 up at 500 kbit/s.
     * @return true when the controller answered.
     */
    bool init();

    /**
     * @brief  Take ONE snapshot and send BOTH frames from it.
     * @note   One snapshot for both, so the two frames describe the same
     *         instant and the mutex is taken once per period instead of
     *         twice.
     * @return None.
     */
    void update();

private:
    /**
     * @brief  Encode the system status frame, 0x200.
     * @param[in]  snaps  Snapshot taken by update().
     * @param[out] out    Eight payload bytes.
     * @return None.
     */
    void buildStatus(const BmsPackState *snaps, uint8_t *out);

    /**
     * @brief  Encode the next pack of the multiplexed frame, 0x201.
     * @param[in]  snaps  Snapshot taken by update().
     * @param[out] out    Eight payload bytes.
     * @return None.
     */
    void buildPackDetail(const BmsPackState *snaps, uint8_t *out);

    /**
     * @brief  Send one frame, re-initialising after repeated failures.
     * @param[in] id    Standard identifier.
     * @param[in] data  Eight payload bytes.
     * @return None.
     */
    void txFrame(uint32_t id, const uint8_t *data);

    MCP_CAN mcp_;
    bool    isReady_;
    bool    seenAll_;       /**< have all packs been seen at least once?
                             *   tells INIT apart from FAULT               */
    uint8_t aliveCounter_;
    uint8_t muxIndex_;      /**< pack to send in the next 0x201            */
    uint8_t txFailRun_;     /**< CONSECUTIVE transmit failures             */
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  FreeRTOS entry point for the vehicle CAN transmitter.
 * @param[in] pvParameters  Unused, required by the FreeRTOS signature.
 * @return None.
 */
void Task_VehicleCAN_Run(void *pvParameters);

/**********************
 *      MACROS
 **********************/

#endif /*TASK_VEHICLE_CAN_H_*/
