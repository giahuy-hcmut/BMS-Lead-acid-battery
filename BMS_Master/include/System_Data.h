/**
 * @file      System_Data.h
 * @author    Do Dang Gia Huy
 * @brief     The single store of measured state, shared by every task.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Despite the name this is not a bag of globals to read at will:
 *   - write it through the System_Update_*() functions, one owner each,
 *   - read it through System_Get_Snapshot(), which returns a private copy.
 *
 * Why a mutex here and none on the slave: the master runs preemptive
 * FreeRTOS on two cores, so two tasks really can touch the same field at the
 * same instant. The slave's cooperative scheduler cannot interleave tasks at
 * all, so there a critical section is only needed against an ISR.
 *
 * Naming note: this layer keeps its C-style System_*() names rather than the
 * camelCase of the C++ style guide. It is deliberately procedural - a data
 * store with no object identity - and the report refers to these names in
 * its data-flow figures.
 */

#ifndef SYSTEM_DATA_H_
#define SYSTEM_DATA_H_

/**********************
 *      INCLUDES
 *********************/

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "Config.h"

/**********************
 *      DEFINES
 *********************/

/** Capacity of the fault-reason string, shared with Task_Logic so the two
 * cannot disagree about the buffer size. */
constexpr size_t kFaultReasonLen = 128U;

/**********************
 *      TYPEDEFS
 **********************/

/** Everything known about one battery. */
struct BmsPackState
{
    float    voltage;       /**< Terminal voltage, V                       */
    float    current;       /**< Pack current, A - the same through every
                             *   battery of a series string                */
    int      soc;           /**< State of charge, 0..100 %, from the
                             *   slave's own Kalman filter                 */
    int8_t   temperature;   /**< Battery temperature, C, offset removed    */
    uint8_t  status;        /**< Slave fault bitmask, 0x00 = healthy       */
    uint32_t lastUpdate;    /**< millis() of the last frame received       */
    bool     isConnected;   /**< Recomputed on every snapshot from
                             *   lastUpdate and kSlaveTimeoutMs            */
};

/** One decoded CAN frame on its way from the RX task to the store. */
struct BmsMessage
{
    uint32_t can_id;
    float    voltage;
    int8_t   temperature;
    uint8_t  status;
    uint8_t  soc;           /**< byte 4, the slave's Kalman SOC */
};

/** Telemetry broadcast to the handheld remote over ESP-NOW.
 * Packed and sized from kTotalPacks so changing the pack count cannot break
 * the layout silently. 47 bytes at five packs. */
struct __attribute__((packed)) BmsTelemetryPacket
{
    float   totalVoltage;               /**< 4 bytes  */
    float   systemCurrent;              /**< 4 bytes  */
    int     systemSOC;                  /**< 4 bytes, -1 when a slave is lost */
    float   packVolts[kTotalPacks];     /**< 20 bytes */
    int8_t  packTemps[kTotalPacks];     /**< 5 bytes  */
    uint8_t packStatus[kTotalPacks];    /**< 5 bytes  */
    bool    isOnline[kTotalPacks];      /**< 5 bytes  */
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/* --- Shared objects ---
 * The store itself and its mutex are private to System_Data.cpp; nothing
 * outside ever referred to them, so nothing outside can now.
 *
 * canQueue stays visible because Task_CAN posts to it and Task_Ingest reads
 * from it. Hiding it would mean wrapping xQueueSend and xQueueReceive in a
 * pair of functions that add nothing but a layer. */
extern QueueHandle_t canQueue;

/** Protection state. Set by Task_Logic, read by Task_WebServer and
 * Task_VehicleCAN. One writer, several readers, so volatile and no mutex. */
extern volatile bool systemLocked;

/** Why the relay opened; several reasons joined by " | ". */
extern char faultReason[kFaultReasonLen];

/** The current sensor is gone: I2C failed to ACK kCurrentFaultLimit times in
 * a row. Written by Task_Current, read by Task_Logic and Task_VehicleCAN.
 *
 * Same shape as systemLocked: one writer, several readers, volatile, no
 * mutex.
 *
 * Deliberately NOT a field of BmsPackState: this is state of the WHOLE
 * system, not of one battery, and putting it in a per-pack struct would mean
 * writing the same value five times. */
extern volatile bool currentSensorFault;

/**
 * @brief  Create the mutex and the CAN queue, and clear the store.
 * @note   Call once from setup(), before any task is created.
 * @return None.
 */
void System_Data_Init();

/**
 * @brief  Store one decoded frame against the pack that sent it.
 * @param  can_id   Identifier the frame arrived on.
 * @param  voltage  Terminal voltage, V.
 * @param  temp     Temperature, C.
 * @param  status   Slave fault bitmask.
 * @param  soc      State of charge, 0..100 %.
 * @note   A frame from an identifier outside the monitored range is ignored:
 *         a slave beyond the configured count is still physically on the bus
 *         and still transmits.
 * @return None.
 */
void System_Update_Pack(uint32_t can_id, float voltage, int8_t temp,
                        uint8_t status, uint8_t soc);

/**
 * @brief  Store the pack current, which is the same for every battery.
 * @param  current  Current in amperes, positive while discharging.
 * @return None.
 */
void System_Update_Current(float current);

/**
 * @brief  Copy the whole store out in one consistent read.
 * @param  snapshotArray  Destination, at least kTotalPacks entries.
 * @note   Also recomputes isConnected for every pack from its lastUpdate.
 * @return None.
 */
void System_Get_Snapshot(BmsPackState *snapshotArray);

/**
 * @brief  Pack current as a plain scalar.
 * @note   Light and NON-BLOCKING, for the 5 ms frame path.
 *         System_Get_Snapshot() takes the mutex and walks all five packs,
 *         which is far too heavy to do every 5 ms.
 * @return Last known pack current, A.
 */
float System_Get_Current(void);

/**
 * @brief  Number of batteries currently monitored.
 * @note   Today this is kTotalPacks. It exists as the hook for the planned
 *         "set the pack count from the web UI" feature: that change then
 *         edits this one function body and adds a setter, instead of hunting
 *         down loops spread over six files.
 * @return Pack count, 1..kTotalPacks.
 */
uint8_t System_GetPackCount(void);

/**
 * @brief  System state of charge, the LOWEST of the packs.
 * @param  snaps  Snapshot the caller already took.
 * @note   In a series string the WEAKEST battery sets the discharge limit,
 *         so taking globalPacks[0] as representative is wrong unless pack 0
 *         happens to be the weakest.
 *
 *         Pure function: takes no mutex, works on the caller's snapshot.
 * @return 0..100, or -1 if ANY pack is offline. A missing pack might be the
 *         weakest one, so the honest answer is "unknown" rather than an
 *         optimistic guess. Callers must handle -1: the UI shows "--" and
 *         Task_Logic refuses to close the relay.
 */
int System_MinSoc(const BmsPackState *snaps);

/**
 * @brief  Sum of the voltages of the packs that are ONLINE.
 * @param  snaps  Snapshot the caller already took.
 * @note   Offline packs are skipped: their stored voltage is a stale number
 *         and adding it produces a falsely high total. The deleted Task_LCD
 *         once forgot to check isConnected and reported a total different
 *         from the web page - exactly what happens when every task rewrites
 *         the same loop. Keeping it in one place stops that recurring with
 *         the next task.
 *
 *         Pure function, same shape as System_MinSoc().
 * @return Total voltage of the online packs, V.
 */
float System_TotalVoltage(const BmsPackState *snaps);

/* --- Web control flags ---
 * Private statics in System_Data.cpp reached only through these accessors.
 * Written by Task_WebServer, read by Task_Logic, Task_CAN and
 * Task_VehicleCAN. A 32-bit bool is read and written atomically on the
 * ESP32, so no mutex is needed. */

/**
 * @brief  Operator override: force the relay open from the web UI.
 * @param  off  true keeps the relay open regardless of everything else.
 * @return None.
 */
void System_Set_RelayOverride(bool off);

/**
 * @brief  Read the operator relay override.
 * @return true while the operator is holding the relay open.
 */
bool System_Get_RelayOverride(void);

/**
 * @brief  Allow or stop transmission of the 0x100 frame.
 * @param  on  false stops it, which lets the slaves fall asleep.
 * @return None.
 */
void System_Set_SlavesActive(bool on);

/**
 * @brief  Read whether the 0x100 frame is being transmitted.
 * @return true while the slaves are being kept awake.
 */
bool System_Get_SlavesActive(void);

/**********************
 *      MACROS
 **********************/

#endif /*SYSTEM_DATA_H_*/
