/**
 * @file      Task_VehicleCAN.cpp
 * @author    Do Dang Gia Huy
 * @brief     Frame encoding and MCP2515 transmission for the vehicle bus.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_VehicleCAN.h"

#include <SPI.h>

/**********************
 *      DEFINES
 **********************/

/** MCP2515 uses the hardware SS line of the SPI bus, not a software one. */
constexpr int8_t kSpiNoSsPin = -1;

/** Standard frame, not extended, as the mcp_can API spells it. */
constexpr uint8_t kMcpStandardFrame = 0U;

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Bind the driver to its chip-select pin; touches no hardware.
 * @param[in] csPin  SPI chip select of the MCP2515.
 */
VehicleCanManager::VehicleCanManager(uint8_t csPin)
    : mcp_(csPin),
      isReady_(false),
      seenAll_(false),
      aliveCounter_(0U),
      muxIndex_(0U),
      txFailRun_(0U)
{
}

/**
 * @brief  Start SPI and bring the MCP2515 up at 500 kbit/s.
 * @return true when the controller answered.
 */
bool VehicleCanManager::init()
{
    SPI.begin(kPinVcanSck, kPinVcanMiso, kPinVcanMosi, kSpiNoSsPin);

    /* MCP_8MHZ must match the crystal printed on the board; X1 reads
     * "8.000" on the HW-184. Declaring MCP_16MHZ, as most examples online
     * do, makes the bit timing wrong by exactly a factor of two, so the bit
     * rate halves and the bus goes COMPLETELY SILENT with no error
     * anywhere. This is the number one mistake with the MCP2515. Watching
     * CNF1/2/3 on the SPI lines with a logic analyzer shows it at once. */
    if (mcp_.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK)
    {
        Serial.println("[VCAN] MCP2515 begin FAILED - check SPI wiring and supply");
        isReady_ = false;
        return false;
    }

#ifdef VCAN_LOOPBACK
    mcp_.setMode(MCP_LOOPBACK);     /* bring-up without any bus */
    Serial.println("[VCAN] MCP2515 ready - LOOPBACK");
#else
    mcp_.setMode(MCP_NORMAL);
    Serial.println("[VCAN] MCP2515 ready - 500 kbit/s, 8 MHz crystal");
#endif

    isReady_ = true;
    return true;
}

/**
 * @brief  Take ONE snapshot and send BOTH frames from it.
 * @return None.
 */
void VehicleCanManager::update()
{
    BmsPackState snaps[kTotalPacks];
    uint8_t      data[kVcanFrameDlc];

    if (!isReady_)
    {
        return;
    }

    /* ONE snapshot for both frames: System_Get_Snapshot() takes the mutex,
     * and calling it twice per period locks twice for nothing. It also
     * guarantees both frames describe the SAME instant. */
    System_Get_Snapshot(snaps);

    buildStatus(snaps, data);
    txFrame(kVcanStatusId, data);

    buildPackDetail(snaps, data);
    txFrame(kVcanPackId, data);
}

/**
 * @brief  FreeRTOS entry point for the vehicle CAN transmitter.
 * @param[in] pvParameters  Unused.
 * @return None.
 */
void Task_VehicleCAN_Run(void *pvParameters)
{
    VehicleCanManager vcan(kPinVcanCs);
    TickType_t        last;

    (void)pvParameters;

    if (!vcan.init())
    {
        Serial.println("[VCAN] init failed -> delete task");
        vTaskDelete(NULL);
    }

    /* vTaskDelayUntil, NOT vTaskDelay: the latter measures the period from
     * when the work FINISHES, so the execution time is added to every cycle
     * and the cadence drifts. The same class of bug the slave scheduler had
     * to fix, by re-arming a task BEFORE running its body. */
    last = xTaskGetTickCount();

    for (;;)
    {
        vcan.update();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(kVcanPeriodMs));
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  Encode the system status frame, 0x200.
 * @param[in]  snaps  Snapshot taken by update().
 * @param[out] out    Eight payload bytes.
 * @return None.
 */
void VehicleCanManager::buildStatus(const BmsPackState *snaps, uint8_t *out)
{
    /* Everything is derived from the snapshot this task already holds. No
     * field is added to System_Data: encoding is the task's job, the store
     * only keeps data. */
    float        totalV    = System_TotalVoltage(snaps);  /* offline skipped */
    float        packI     = snaps[0].current;   /* same through a series pack */
    int          soc       = System_MinSoc(snaps);
    uint8_t      flags     = 0U;
    bool         allOnline = true;
    VCAN_State_t state;
    uint16_t     vRaw;
    int16_t      iRaw;

    /* Is the current sensor alive? Task_Current raises this flag after
     * kCurrentFaultLimit consecutive I2C transactions without an ACK. Once
     * it is lost, packI is a stale value sitting in the store and must not
     * be used to infer anything. */
    bool iValid = !currentSensorFault;

    for (uint8_t i = 0U; i < System_GetPackCount(); i++)
    {
        if (snaps[i].isConnected)
        {
            flags |= snaps[i].status;   /* the slave uses bits 0-2 only */
        }
        else
        {
            allOnline = false;          /* do NOT OR a stale status in */
        }
    }

    /* INIT until every pack has been seen once. After that, losing one is a
     * FAULT, not "still starting up". */
    if (allOnline)
    {
        seenAll_ = true;
    }

    if (!allOnline)
    {
        flags |= VCAN_FLAG_SLAVE_LOST;
    }

    if (systemLocked)
    {
        flags |= VCAN_FLAG_RELAY_OPEN;
    }

    if (System_Get_RelayOverride())
    {
        flags |= VCAN_FLAG_MANUAL_OFF;
    }

    if (currentSensorFault)
    {
        flags |= VCAN_FLAG_BMS_INTERNAL;
    }

    /* Report over-current ONLY while the reading can be trusted. Reporting
     * it from an invalid value is worse than not reporting at all: the
     * vehicle would brake for a garbage number. */
    if (iValid && (packI > kMaxDischargeCurrentA))
    {
        flags |= VCAN_FLAG_OVER_CURRENT;
    }

    /* !iValid means FAULT, not IDLE. Task_Logic will open the relay for the
     * lost sensor too, but it only scans every kProtectionPeriodMs; inside
     * that window systemLocked is still false and packI, the stale value,
     * is very likely inside the dead band - so the vehicle would be told
     * "IDLE" while the pack is discharging and the BMS is blind.
     *
     * kCurrentIdleBandA is the dead band Task_Current already applies: below
     * it the current is forced to exactly 0.0, so reusing THAT constant as
     * the "at rest" boundary avoids a second number that could drift. */
    if (!seenAll_)
    {
        state = VCAN_STATE_INIT;
    }
    else if (systemLocked || !iValid)
    {
        state = VCAN_STATE_FAULT;
    }
    else if (packI > kCurrentIdleBandA)
    {
        state = VCAN_STATE_DISCHARGE;
    }
    else if (packI < -kCurrentIdleBandA)
    {
        state = VCAN_STATE_CHARGE;
    }
    else
    {
        state = VCAN_STATE_IDLE;
    }

    /* Clamp BEFORE scaling, for the same reason as sendCurrentFrame(): an
     * int16 only reaches +/-327.67 A, so a faulty sensor would overflow and
     * FLIP THE SIGN - the vehicle would believe the pack is charging while
     * it is being discharged. */
    if (packI > kCurrentClampA)
    {
        packI = kCurrentClampA;
    }

    if (packI < -kCurrentClampA)
    {
        packI = -kCurrentClampA;
    }

    vRaw = (uint16_t)lroundf(totalV * kScaleX100);
    iRaw = iValid ? (int16_t)lroundf(packI * kScaleX100)
                  : VCAN_CURRENT_INVALID;

    out[0] = (uint8_t)((vRaw >> 8) & kByteMask);        /* MSB first */
    out[1] = (uint8_t)(vRaw & kByteMask);
    out[2] = (uint8_t)(((uint16_t)iRaw >> 8) & kByteMask);
    out[3] = (uint8_t)((uint16_t)iRaw & kByteMask);
    out[4] = (soc < 0) ? VCAN_SOC_INVALID : (uint8_t)soc;
    out[5] = (uint8_t)state;
    out[6] = flags;
    out[7] = aliveCounter_++;
}

/**
 * @brief  Encode the next pack of the multiplexed frame, 0x201.
 * @param[in]  snaps  Snapshot taken by update().
 * @param[out] out    Eight payload bytes.
 * @return None.
 */
void VehicleCanManager::buildPackDetail(const BmsPackState *snaps, uint8_t *out)
{
    uint8_t  count = System_GetPackCount();
    uint8_t  idx   = muxIndex_;
    bool     online;
    uint16_t vRaw;

    /* Rotate over count, not over kTotalPacks: once the pack count becomes
     * settable from the web UI, the sweep shrinks with it instead of sending
     * empty frames for batteries that do not exist. */
    muxIndex_ = (uint8_t)((muxIndex_ + 1U) % count);

    online = snaps[idx].isConnected;
    vRaw   = (uint16_t)lroundf(snaps[idx].voltage * kScaleX100);

    out[0] = idx;
    out[1] = online ? (uint8_t)((vRaw >> 8) & kByteMask) : 0x00U;
    out[2] = online ? (uint8_t)(vRaw & kByteMask) : 0x00U;

    /* For an offline pack the stored voltage, SOC and temperature are all
     * stale, so report them as unknown - the same thing the web page does by
     * showing "--". */
    out[3] = online ? (uint8_t)snaps[idx].soc : VCAN_SOC_INVALID;
    out[4] = online ? (uint8_t)snaps[idx].temperature : (uint8_t)VCAN_TEMP_INVALID;
    out[5] = online ? snaps[idx].status : 0x00U;
    out[6] = online ? 1U : 0U;
    out[7] = count;
}

/**
 * @brief  Send one frame, re-initialising after repeated failures.
 * @param[in] id    Standard identifier.
 * @param[in] data  Eight payload bytes.
 * @return None.
 */
void VehicleCanManager::txFrame(uint32_t id, const uint8_t *data)
{
    /* const_cast because sendMsgBuf() takes a non-const pointer although it
     * only reads the payload; the mcp_can API predates const correctness. */
    if (mcp_.sendMsgBuf(id, kMcpStandardFrame, kVcanFrameDlc,
                        const_cast<uint8_t *>(data)) == CAN_OK)
    {
        txFailRun_ = 0U;
        return;
    }

    /* The MCP2515 does NOT leave bus-off on its own the way the STM32 bxCAN
     * does, where the slave simply enables AutoBusOff and the hardware
     * handles it. Here a run of failures has to re-initialise by hand, or
     * the link to the vehicle stays dead until the power is cycled -
     * silently. */
    txFailRun_++;

    if (txFailRun_ >= kVcanTxFailLimit)
    {
        Serial.println("[VCAN] TX stuck -> re-init MCP2515");
        txFailRun_ = 0U;
        init();
    }
}
