/**
 * @file      main.cpp
 * @author    Do Dang Gia Huy
 * @brief     Start-up and the task schedule of the BMS central controller.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Core 1 carries everything with a deadline, core 0 carries the user
 * interface, so a slow web client can never delay the protection scan.
 *
 *   core  prio  task       period     job
 *   ----  ----  ---------  ---------  ------------------------------------
 *    1     5    CANrx      event      receive slave frames
 *    1     5    CANtx      5 ms       publish the pack current
 *    1     4    Ingest     event      move frames into the store
 *    1     4    Protect    10 ms      safety scan, drives the relay
 *    1     4    Current    3 ms       sample the shunt
 *    1     3    VCAN       100 ms     report to the vehicle
 *    0     2    WebSrv     500 ms     dashboard and controls
 *    0     1    Term       1000 ms    serial console
 *
 * The vehicle CAN task sits below Protect because safety must win, and above
 * WebSrv because it is a control path, not a display.
 */

/**********************
 *      INCLUDES
 **********************/

#include <Arduino.h>

#include "System_Data.h"
#include "Task_CAN.h"
#include "Task_Current.h"
#include "Task_EspNow.h"
#include "Task_Ingest.h"
#include "Task_Logic.h"
#include "Task_Terminal.h"
#include "Task_VehicleCAN.h"
#include "Task_WebServer.h"

/**********************
 *      DEFINES
 **********************/

constexpr uint32_t kSerialBaud = 115200U;

/* Stack sizes, bytes. The web server needs the largest because the async
 * HTTP stack allocates inside the task. */
constexpr uint32_t kStackSmall = 2048U;
constexpr uint32_t kStackNormal = 4096U;
constexpr uint32_t kStackWeb = 8192U;

/* Priorities. Higher runs first. */
constexpr UBaseType_t kPrioCan     = 5U;
constexpr UBaseType_t kPrioControl = 4U;
constexpr UBaseType_t kPrioVehicle = 3U;
constexpr UBaseType_t kPrioWeb     = 2U;
constexpr UBaseType_t kPrioConsole = 1U;

/* Core assignment. */
constexpr BaseType_t kCoreRealtime = 1;
constexpr BaseType_t kCoreUi       = 0;

/** Pause after the real-time tasks are up, before starting the radio, so the
 * Wi-Fi driver does not contend with the first CAN traffic. */
constexpr uint32_t kRadioStartDelayMs = 500U;

/** The ESP-NOW link to the handheld remote. Its code is kept compiling but
 * the task is not started; set this to true to bring the link back. */
constexpr bool kEspNowEnabled = false;

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
 * @brief  Bring the store up and create every task.
 * @return None.
 */
void setup()
{
    Serial.begin(kSerialBaud);

    Serial.printf("\n\n>>> Telemetry packet size: %u bytes <<<\n\n",
                  (unsigned)sizeof(BmsTelemetryPacket));

    System_Data_Init();

    /* --- Core 1: the real-time work ---------------------------------- */

    /* CAN is split in two. Rx blocks on the interrupt, which makes it event
     * driven; Tx keeps a fixed 5 ms cadence. In one task the two fight:
     * blocking to receive means the send never gets its turn, which is why
     * the earlier version had to poll a thousand times a second. */
    xTaskCreatePinnedToCore(Task_CAN_Rx_Run, "CANrx", kStackNormal, NULL,
                            kPrioCan, NULL, kCoreRealtime);
    xTaskCreatePinnedToCore(Task_CAN_Tx_Run, "CANtx", kStackSmall, NULL,
                            kPrioCan, NULL, kCoreRealtime);

    /* Ingest copies frames into the store, event driven. Protect scans for
     * faults on its own fixed cadence, so its timing no longer depends on
     * when frames happen to arrive. */
    xTaskCreatePinnedToCore(Task_Ingest_Run, "Ingest", kStackSmall, NULL,
                            kPrioControl, NULL, kCoreRealtime);
    xTaskCreatePinnedToCore(Task_Logic_Run, "Protect", kStackNormal, NULL,
                            kPrioControl, NULL, kCoreRealtime);
    xTaskCreatePinnedToCore(Task_Current_Run, "Current", kStackNormal, NULL,
                            kPrioControl, NULL, kCoreRealtime);

    /* Vehicle CAN over the MCP2515. Disabled while measuring error: it
     * prints about four lines a second with no vehicle node attached, which
     * would pollute the CSV file, and voltage measurement does not need the
     * vehicle bus. */
#ifndef CSV_LOG
    xTaskCreatePinnedToCore(Task_VehicleCAN_Run, "VCAN", kStackNormal, NULL,
                            kPrioVehicle, NULL, kCoreRealtime);
#endif

    /* --- Core 0: the user interface ---------------------------------- */

    /* Kept off core 1 so nothing here can delay a protection decision. */
    xTaskCreatePinnedToCore(Task_Terminal_Run, "Term", kStackNormal, NULL,
                            kPrioConsole, NULL, kCoreUi);

    /* The LCD task is gone for good: the master no longer drives a display,
     * which freed GPIO 21/22 for the INA219. Task_Terminal and Task_EspNow
     * remain - currently idle, but they claim no pin. */
    xTaskCreatePinnedToCore(Task_WebServer_Run, "WebSrv", kStackWeb, NULL,
                            kPrioWeb, NULL, kCoreUi);

    delay(kRadioStartDelayMs);

    if (kEspNowEnabled)
    {
        xTaskCreatePinnedToCore(Task_EspNow_Run, "EspNowTx", kStackNormal,
                                NULL, kPrioWeb, NULL, kCoreUi);
    }
}

/**
 * @brief  Arduino main loop, deliberately retired.
 * @note   FreeRTOS owns the schedule, so the loop task is deleted to give
 *         its stack back.
 * @return None.
 */
void loop()
{
    vTaskDelete(NULL);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
