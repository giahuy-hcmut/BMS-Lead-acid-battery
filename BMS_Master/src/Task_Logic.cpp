/**
 * @file      Task_Logic.cpp
 * @author    Do Dang Gia Huy
 * @brief     Fault collection, relay actuation and the protection scan loop.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_Logic.h"

#include "System_Data.h"

#include <string.h>

/**********************
 *      DEFINES
 **********************/

/** Separator between reasons in the fault string. */
#define FAULT_REASON_SEPARATOR  " | "

/** Longest single reason tag, "HwErr:0x107" plus slack. */
constexpr size_t kReasonTagLen = 24U;

/** Wait before the first scan, so the slaves have time to boot and send
 * their first frame. Without it every pack looks offline and the relay would
 * trip for nothing at power-up. */
constexpr TickType_t kStartupGrace = pdMS_TO_TICKS(2000);

/** Healthy slave status byte, matching ERROR_NONE on the slave side. */
constexpr uint8_t kSlaveStatusHealthy = 0x00U;

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

namespace
{
void addReason(char *buf, size_t cap, const char *txt);
}

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
 * @brief  Start unlatched; touches no hardware.
 */
LogicManager::LogicManager()
    : isSystemLocked_(false)
{
}

/**
 * @brief  Configure the relay pin and leave the relay OPEN.
 * @return None.
 */
void LogicManager::init()
{
    pinMode(kPinRelayControl, OUTPUT);
    digitalWrite(kPinRelayControl, kRelayOff);

    Serial.println("[LOGIC] Manager initialised, relay configured OPEN");
}

/**
 * @brief  Collect EVERY active fault and set the relay accordingly.
 * @return None.
 */
void LogicManager::evaluateProtection()
{
    BmsPackState snaps[kTotalPacks];
    char         reasons[kFaultReasonLen];
    int          sysSoc;
    bool         isSafe = true;
    bool         iValid;

    System_Get_Snapshot(snaps);
    reasons[0] = '\0';

    /* The weakest battery limits a series pack. -1 means at least one slave
     * is offline, so the picture is incomplete - see System_MinSoc(). */
    sysSoc = System_MinSoc(snaps);

    /* Current sensor lost: the reading can no longer be trusted, so it must
     * NOT be used for the over-current test below. The INA219 has no ALE pin
     * (the INA226 does), so this software layer is the ONLY protection. */
    iValid = !currentSensorFault;

    if (System_Get_RelayOverride())
    {
        isSafe = false;
        addReason(reasons, sizeof(reasons), "Operator OFF");
    }

    if (!iValid)
    {
        isSafe = false;
        addReason(reasons, sizeof(reasons), "Current Sensor Lost (I2C)");
    }

    /* Name the faulty pack rather than reporting "a fault": with five
     * batteries you have to know WHICH one to repair it. Deliberately no
     * `break` - every fault has to be collected. */
    for (uint8_t i = 0U; i < kTotalPacks; i++)
    {
        char tag[kReasonTagLen];

        if (!snaps[i].isConnected)
        {
            isSafe = false;
            snprintf(tag, sizeof(tag), "Lost:0x%03X", (unsigned)(kCanBaseId + i));
            addReason(reasons, sizeof(reasons), tag);
        }
        else if (snaps[i].status != kSlaveStatusHealthy)
        {
            isSafe = false;
            snprintf(tag, sizeof(tag), "HwErr:0x%03X", (unsigned)(kCanBaseId + i));
            addReason(reasons, sizeof(reasons), tag);
        }
        else
        {
            /* Pack healthy. */
        }
    }

    if (iValid && (snaps[0].current > kMaxDischargeCurrentA))
    {
        isSafe = false;
        addReason(reasons, sizeof(reasons), "Over Current");
    }

    if ((sysSoc >= 0) && (sysSoc < kMinSocShutdownPct))
    {
        isSafe = false;
        addReason(reasons, sizeof(reasons), "Battery Depleted (Low SOC)");
    }

    if (!isSafe)
    {
        lockSystem(reasons);
    }
    else
    {
        /* The relay starts OPEN (s_webForceRelayOff is true), so this path
         * always goes through the hysteresis: after the operator switches ON
         * from the web page, the relay only closes once the SOC is safely
         * above kRecoverySocPct.
         * sysSoc == -1, meaning a pack is offline, is blocked here too, and
         * that is intentional. */
        if (sysSoc >= kRecoverySocPct)
        {
            unlockSystem();
        }
    }
}

/**
 * @brief  FreeRTOS entry point for the safety monitor.
 * @param[in] pvParameters  Unused.
 * @return None.
 */
void Task_Logic_Run(void *pvParameters)
{
    LogicManager myLogic;
    TickType_t   xLastWakeTime;

    (void)pvParameters;

    myLogic.init();
    vTaskDelay(kStartupGrace);

    /* This task now ONLY monitors safety; moving frames into the store went
     * to Task_Ingest. Scanning on a FIXED cadence instead of piggybacking on
     * a queue timeout: the fastest-changing input is the current at 5 ms, so
     * 10 ms is enough and still leaves the CPU to everything else. */
    xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        myLogic.evaluateProtection();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(kProtectionPeriodMs));
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  Open the relay and publish the reason.
 * @param[in] reason  Full reason string, already joined.
 * @return None.
 */
void LogicManager::lockSystem(const char *reason)
{
    digitalWrite(kPinRelayControl, kRelayOff);

    /* Refresh the reason on EVERY call, not just the first: the set of
     * active faults changes over time - another slave drops out, the I2C
     * fault clears - and the web page has to show the present state. */
    strncpy(faultReason, reason, sizeof(faultReason) - 1U);
    faultReason[sizeof(faultReason) - 1U] = '\0';

    if (!isSystemLocked_)
    {
        /* Print only on the TRANSITION, otherwise the log fills up. */
        isSystemLocked_ = true;
        systemLocked    = true;
        Serial.printf("\n[PROTECTION] RELAY TRIPPED! Reason: %s\n", reason);
    }
}

/**
 * @brief  Close the relay and clear the reason.
 * @return None.
 */
void LogicManager::unlockSystem()
{
    digitalWrite(kPinRelayControl, kRelayOn);

    if (isSystemLocked_)
    {
        isSystemLocked_ = false;
        systemLocked    = false;
        faultReason[0]  = '\0';
        Serial.println("\n[PROTECTION] SYSTEM RECOVERED. RELAY ON.");
    }
}

namespace
{

/**
 * @brief  Append one reason to the string, separated by " | ".
 * @param[in,out] buf  Destination string, already NUL terminated.
 * @param[in]     cap  Capacity of @p buf including the terminator.
 * @param[in]     txt  Reason to append.
 * @note   Collecting SEVERAL reasons instead of keeping only the first: when
 *         I2C and a CAN node fail together, the web page must show BOTH.
 * @return None.
 */
void addReason(char *buf, size_t cap, const char *txt)
{
    size_t n = strlen(buf);

    if (n > 0U)
    {
        strncat(buf, FAULT_REASON_SEPARATOR, cap - n - 1U);
        n = strlen(buf);
    }

    strncat(buf, txt, cap - n - 1U);
}

}  /* namespace */
