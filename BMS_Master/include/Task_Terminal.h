/**
 * @file      Task_Terminal.h
 * @author    Do Dang Gia Huy
 * @brief     Serial dashboard, and the CSV logger used for error measurement.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Two output modes selected at build time by CSV_LOG: a human-readable
 * table, or comma-separated rows for a spreadsheet.
 */

#ifndef TASK_TERMINAL_H_
#define TASK_TERMINAL_H_

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

/** Renders the system state to the serial console. */
class TerminalDashboard
{
public:
    /**
     * @brief  Clear the local cache; touches no hardware.
     */
    TerminalDashboard();

    /**
     * @brief  Print the banner and wait for the slaves to come up.
     * @return None.
     */
    void init();

    /**
     * @brief  Refresh the console once a second, forever.
     * @return None.
     */
    void run();

private:
    /**
     * @brief  Take a consistent copy of the store.
     * @note   isConnected is already computed by System_Get_Snapshot(), so
     *         recomputing it here would be pure duplication.
     * @return None.
     */
    void fetchData();

    /**
     * @brief  Print the totals and the table heading.
     * @param[in] uptime  Seconds since reset.
     * @return None.
     */
    void printHeader(uint32_t uptime);

    /**
     * @brief  Print one pack row.
     * @param[in] index  Pack index, used to derive its CAN identifier.
     * @param[in] pack   That pack's state.
     * @return None.
     */
    void printRow(uint8_t index, const BmsPackState &pack);

    /**
     * @brief  Print the closing rule.
     * @return None.
     */
    void printFooter();

    /** Local cache, so the whole frame is rendered from one instant. */
    BmsPackState localPacks_[kTotalPacks];
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  FreeRTOS entry point for the serial dashboard.
 * @param[in] pvParameters  Unused, required by the FreeRTOS signature.
 * @return None.
 */
void Task_Terminal_Run(void *pvParameters);

/**********************
 *      MACROS
 **********************/

#endif /*TASK_TERMINAL_H_*/
