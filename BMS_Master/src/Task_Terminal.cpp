/**
 * @file      Task_Terminal.cpp
 * @author    Do Dang Gia Huy
 * @brief     Console rendering and the CSV logging path.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_Terminal.h"

/**********************
 *      DEFINES
 **********************/

/** Refresh period of the console and of one CSV row. The slaves only send a
 * frame every 1000 ms, so printing faster adds duplicate rows and no
 * information. */
constexpr TickType_t kRefreshPeriod = pdMS_TO_TICKS(1000);

/** Let the slaves boot and send their first frames before printing. Kept in
 * CSV mode too, for the same reason. */
constexpr TickType_t kStartupDelay = pdMS_TO_TICKS(1000);

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
 * @brief  Clear the local cache; touches no hardware.
 */
TerminalDashboard::TerminalDashboard()
    : localPacks_{}
{
}

/**
 * @brief  Print the banner and wait for the slaves to come up.
 * @return None.
 */
void TerminalDashboard::init()
{
#ifndef CSV_LOG
    Serial.println("\n\n");
    Serial.println(">>> BMS MASTER CONSOLE V3.0 <<<");
    Serial.println(">>> TEST MODE: 2 PACKS LITHIUM <<<");
#endif

    vTaskDelay(kStartupDelay);
}

/**
 * @brief  Refresh the console once a second, forever.
 * @return None.
 */
void TerminalDashboard::run()
{
    init();

    while (1)
    {
        fetchData();

#ifdef CSV_LOG
        /* Error-measurement mode. Commas so a spreadsheet splits the
         * columns, padding so the rows still line up when read straight off
         * the terminal.
         *
         * The column names are derived from kCanBaseId, so they cannot
         * disagree with reality: v_103 is the battery of SLAVE_INDEX 0,
         * v_104 is SLAVE_INDEX 1, and so on. */
        static bool s_hdr = false;

        if (!s_hdr)
        {
            Serial.print("  millis");
            for (uint8_t i = 0U; i < kTotalPacks; i++)
            {
                Serial.printf(", v_%03X", (unsigned)(kCanBaseId + i));
            }
            for (uint8_t i = 0U; i < kTotalPacks; i++)
            {
                Serial.printf(", s_%03X", (unsigned)(kCanBaseId + i));
            }
            Serial.println();
            s_hdr = true;
        }

        Serial.printf("%8lu", millis());
        for (uint8_t i = 0U; i < kTotalPacks; i++)
        {
            Serial.printf(", %6.2f", localPacks_[i].voltage);
        }
        for (uint8_t i = 0U; i < kTotalPacks; i++)
        {
            Serial.printf(", %5d", localPacks_[i].soc);
        }
        Serial.println();
#else
        printHeader(millis() / 1000U);

        for (uint8_t i = 0U; i < kTotalPacks; i++)
        {
            printRow(i, localPacks_[i]);
        }

        printFooter();
#endif

        vTaskDelay(kRefreshPeriod);
    }
}

/**
 * @brief  FreeRTOS entry point for the serial dashboard.
 * @param[in] pvParameters  Unused.
 * @return None.
 */
void Task_Terminal_Run(void *pvParameters)
{
    TerminalDashboard dashboard;

    (void)pvParameters;

    dashboard.run();
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  Take a consistent copy of the store.
 * @return None.
 */
void TerminalDashboard::fetchData()
{
    System_Get_Snapshot(localPacks_);
}

/**
 * @brief  Print the totals and the table heading.
 * @param[in] uptime  Seconds since reset.
 * @return None.
 */
void TerminalDashboard::printHeader(uint32_t uptime)
{
    float totalVolt = System_TotalVoltage(localPacks_);
    int   sysSOC    = System_MinSoc(localPacks_);

    Serial.println("\n==========================================================");
    Serial.printf("   BMS MASTER DASHBOARD (Uptime: %lu s)\n", uptime);
    Serial.println("==========================================================");

    /* localPacks_[0].current is the current of the WHOLE system, since the
     * same current flows through batteries in series. SOC is NOT like that:
     * localPacks_[i].soc belongs to battery i alone, computed by the Kalman
     * filter on that slave, so the system SOC has to come from
     * System_MinSoc(). */
    Serial.printf(" SYSTEM VOLTAGE: %6.2f V  |  CURRENT: %6.2f A\n",
                  totalVolt, localPacks_[0].current);

    if (sysSOC < 0)
    {
        /* A slave is offline, so there is no way to tell whether the
         * missing battery is the weakest one. */
        Serial.printf(" SYSTEM SOC    :  --  %%       |  MODE   : TEST (2S)\n");
    }
    else
    {
        Serial.printf(" SYSTEM SOC    : %3d %%       |  MODE   : TEST (2S)\n",
                      sysSOC);
    }

    Serial.println("----------------------------------------------------------");
    Serial.println("| ID    | VOLTAGE | TEMP  | ERR  | STATUS      | UPDATED |");
    Serial.println("|-------|---------|-------|------|-------------|---------|");
}

/**
 * @brief  Print one pack row.
 * @param[in] index  Pack index, used to derive its CAN identifier.
 * @param[in] pack   That pack's state.
 * @return None.
 */
void TerminalDashboard::printRow(uint8_t index, const BmsPackState &pack)
{
    unsigned canID = (unsigned)(kCanBaseId + index);

    Serial.printf("| 0x%03X | ", canID);

    if (pack.isConnected)
    {
        Serial.printf("%6.2f V | %3d C | 0x%02X | [ONLINE] \xE2\x9C\x85 | %4lu ms |\n",
                      pack.voltage, pack.temperature, pack.status,
                      millis() - pack.lastUpdate);
    }
    else
    {
        Serial.printf(" --.-- V |  -- C | ---- | [LOST]   \xE2\x9D\x8C |  ----   |\n");
    }
}

/**
 * @brief  Print the closing rule.
 * @return None.
 */
void TerminalDashboard::printFooter()
{
    Serial.println("==========================================================");
}
