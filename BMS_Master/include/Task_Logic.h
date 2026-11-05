/**
 * @file      Task_Logic.h
 * @author    Do Dang Gia Huy
 * @brief     Safety monitor: scans every fault source and drives the relay.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * This is the only place in the firmware that opens or closes the contactor.
 * It scans on a fixed cadence rather than reacting to frame arrival, so the
 * worst-case detection-to-decision interval is one kProtectionPeriodMs.
 */

#ifndef TASK_LOGIC_H_
#define TASK_LOGIC_H_

/**********************
 *      INCLUDES
 *********************/

#include <Arduino.h>

#include "System_Data.h"

/**********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/** Owns the relay pin and the latched protection state. */
class LogicManager
{
public:
    /**
     * @brief  Start unlatched; touches no hardware.
     */
    LogicManager();

    /**
     * @brief  Configure the relay pin and leave the relay OPEN.
     * @note   Opening at start-up is deliberate: the system must never
     *         energise before the operator asks for it.
     * @return None.
     */
    void init();

    /**
     * @brief  Collect EVERY active fault and set the relay accordingly.
     * @note   Gathers all reasons rather than stopping at the first, so the
     *         web page shows the complete picture when several faults
     *         coincide. Closing again uses hysteresis: kMinSocShutdownPct to
     *         open, kRecoverySocPct to close.
     * @return None.
     */
    void evaluateProtection();

private:
    /**
     * @brief  Open the relay and publish the reason.
     * @param[in] reason  Full reason string, already joined.
     * @return None.
     */
    void lockSystem(const char *reason);

    /**
     * @brief  Close the relay and clear the reason.
     * @return None.
     */
    void unlockSystem();

    bool isSystemLocked_;
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  FreeRTOS entry point for the safety monitor.
 * @param[in] pvParameters  Unused, required by the FreeRTOS signature.
 * @return None.
 */
void Task_Logic_Run(void *pvParameters);

/**********************
 *      MACROS
 **********************/

#endif /*TASK_LOGIC_H_*/
