/**
 * @file      System_Data.cpp
 * @author    Do Dang Gia Huy
 * @brief     Storage, mutex discipline and the pure helpers over the store.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "System_Data.h"

#include <string.h>

/**********************
 *      DEFINES
 **********************/

/** How long a writer waits for the mutex before giving up. */
constexpr TickType_t kStoreMutexWait = pdMS_TO_TICKS(100);

/** Above any valid SOC, so the first comparison always takes. */
constexpr int kSocSentinelHigh = 101;

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/* The store and its mutex are private to this file. */
static BmsPackState      s_globalPacks[kTotalPacks];
static SemaphoreHandle_t s_dataMutex;

/* Shared with Task_CAN and Task_Ingest, declared extern in the header. */
QueueHandle_t canQueue;

volatile bool systemLocked               = false;
/* Private: reached only through System_Set_FaultReason() and
 * System_Get_FaultReason(), both of which take the store mutex. */
static char s_faultReason[kFaultReasonLen] = "";

/* Initialised TRUE on purpose: fail-safe. Until there is evidence that the
 * current sensor is alive, treat it as lost; CurrentSensorManager::init()
 * clears the flag once ina.begin() succeeds. The relay also starts OPEN, in
 * LogicManager::init(), so there is no window between the two. */
volatile bool currentSensorFault = true;

/* Initialised TRUE: the relay is ALWAYS open at power-up. The operator has
 * to switch it on from the web page before the system energises - it never
 * closes by itself just because the conditions happen to be satisfied. That
 * is both fail-safe and the intended way to operate the vehicle. */
static volatile bool s_webForceRelayOff = true;
static volatile bool s_webSlavesActive  = true;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Create the mutex and the CAN queue, and clear the store.
 * @return None.
 */
void System_Data_Init()
{
    s_dataMutex = xSemaphoreCreateMutex();
    canQueue  = xQueueCreate(kCanQueueLength, sizeof(BmsMessage));

    /* The WHOLE array, not just the monitored count: entries beyond the
     * count must not hold indeterminate values either. */
    for (uint8_t i = 0U; i < kTotalPacks; i++)
    {
        s_globalPacks[i].voltage     = 0.0f;
        s_globalPacks[i].current     = 0.0f;
        s_globalPacks[i].soc         = 0;
        s_globalPacks[i].temperature = 0;
        s_globalPacks[i].status      = 0U;
        s_globalPacks[i].lastUpdate  = 0U;
        s_globalPacks[i].isConnected = false;
    }
}

/**
 * @brief  Store one decoded frame against the pack that sent it.
 * @param  can_id   Identifier the frame arrived on.
 * @param  voltage  Terminal voltage, V.
 * @param  temp     Temperature, C.
 * @param  status   Slave fault bitmask.
 * @param  soc      State of charge, 0..100 %.
 * @return None.
 */
void System_Update_Pack(uint32_t can_id, float voltage, int8_t temp,
                        uint8_t status, uint8_t soc)
{
    int idx = (int)can_id - (int)kCanBaseId;

    /* Bound against the MONITORED count, not the array extent: a slave
     * beyond the configured count is still physically on the bus and still
     * sends frames, and those must be ignored. */
    if ((idx < 0) || (idx >= (int)System_GetPackCount()))
    {
        return;
    }

    if (xSemaphoreTake(s_dataMutex, kStoreMutexWait) == pdTRUE)
    {
        s_globalPacks[idx].voltage     = voltage;
        s_globalPacks[idx].temperature = temp;
        s_globalPacks[idx].status      = status;
        s_globalPacks[idx].soc         = soc;
        s_globalPacks[idx].lastUpdate  = millis();
        xSemaphoreGive(s_dataMutex);
    }
}

/**
 * @brief  Store the pack current, which is the same for every battery.
 * @param  current  Current in amperes, positive while discharging.
 * @return None.
 */
void System_Update_Current(float current)
{
    if (xSemaphoreTake(s_dataMutex, kStoreMutexWait) == pdTRUE)
    {
        for (uint8_t i = 0U; i < kTotalPacks; i++)
        {
            s_globalPacks[i].current = current;
        }
        xSemaphoreGive(s_dataMutex);
    }
}

/**
 * @brief  Copy the whole store out in one consistent read.
 * @param  snapshotArray  Destination, at least kTotalPacks entries.
 * @return None.
 */
void System_Get_Snapshot(BmsPackState *snapshotArray)
{
    if (xSemaphoreTake(s_dataMutex, kStoreMutexWait) == pdTRUE)
    {
        /* The WHOLE array: the caller sized its buffer at kTotalPacks and
         * must not be left reading uninitialised entries past the count. */
        for (uint8_t i = 0U; i < kTotalPacks; i++)
        {
            /* Online state is derived here, at the one place that owns
             * lastUpdate, so no caller has to recompute it. */
            bool isOnline = (s_globalPacks[i].lastUpdate > 0U) &&
                            ((millis() - s_globalPacks[i].lastUpdate) < kSlaveTimeoutMs);

            s_globalPacks[i].isConnected = isOnline;
            snapshotArray[i]           = s_globalPacks[i];
        }
        xSemaphoreGive(s_dataMutex);
    }
}

/**
 * @brief  Pack current as a plain scalar, non-blocking.
 * @return Last known pack current, A.
 */
float System_Get_Current(void)
{
    static float s_cached = 0.0f;   /* only the CAN TX task calls this */

    /* Timeout 0: this sits on the 5 ms frame path and must never block.
     * On contention keep the last known value rather than returning 0.0, so
     * a busy mutex cannot inject a spurious "0 A" into every slave's filter.
     * s_globalPacks[0] because System_Update_Current() writes the same value
     * to all packs - the same current flows through batteries in series. */
    if (xSemaphoreTake(s_dataMutex, 0) == pdTRUE)
    {
        s_cached = s_globalPacks[0].current;
        xSemaphoreGive(s_dataMutex);
    }

    return s_cached;
}

/**
 * @brief  Number of batteries currently monitored.
 * @return Pack count.
 */
uint8_t System_GetPackCount(void)
{
    return kTotalPacks;     /* runtime-configurable from the web UI later */
}

/**
 * @brief  System state of charge, the LOWEST of the packs.
 * @param  snaps  Snapshot the caller already took.
 * @return 0..100, or -1 if any pack is offline.
 */
int System_MinSoc(const BmsPackState *snaps)
{
    int lowest = kSocSentinelHigh;

    for (uint8_t i = 0U; i < System_GetPackCount(); i++)
    {
        if (!snaps[i].isConnected)
        {
            return -1;      /* incomplete picture - see the header */
        }

        if (snaps[i].soc < lowest)
        {
            lowest = snaps[i].soc;
        }
    }

    return lowest;
}

/**
 * @brief  Sum of the voltages of the packs that are ONLINE.
 * @param  snaps  Snapshot the caller already took.
 * @return Total voltage of the online packs, V.
 */
float System_TotalVoltage(const BmsPackState *snaps)
{
    float total = 0.0f;

    for (uint8_t i = 0U; i < System_GetPackCount(); i++)
    {
        if (snaps[i].isConnected)
        {
            total += snaps[i].voltage;
        }
    }

    return total;
}

/**
 * @brief  Publish why the relay opened.
 * @param[in] reason  Reasons already joined by " | ".
 * @return None.
 */
void System_Set_FaultReason(const char *reason)
{
    if (reason == NULL)
    {
        return;
    }

    if (xSemaphoreTake(s_dataMutex, kStoreMutexWait) == pdTRUE)
    {
        strncpy(s_faultReason, reason, sizeof(s_faultReason) - 1U);
        s_faultReason[sizeof(s_faultReason) - 1U] = '\0';
        xSemaphoreGive(s_dataMutex);
    }
}

/**
 * @brief  Copy out why the relay opened.
 * @param[out] out  Destination, always left NUL terminated.
 * @param[in]  cap  Capacity of @p out including the terminator.
 * @return None.
 */
void System_Get_FaultReason(char *out, size_t cap)
{
    if ((out == NULL) || (cap == 0U))
    {
        return;
    }

    out[0] = '\0';

    if (xSemaphoreTake(s_dataMutex, kStoreMutexWait) == pdTRUE)
    {
        strncpy(out, s_faultReason, cap - 1U);
        out[cap - 1U] = '\0';
        xSemaphoreGive(s_dataMutex);
    }
}

/**
 * @brief  Operator override: force the relay open from the web UI.
 * @param  off  true keeps the relay open regardless of everything else.
 * @return None.
 */
void System_Set_RelayOverride(bool off)
{
    s_webForceRelayOff = off;
}

/**
 * @brief  Read the operator relay override.
 * @return true while the operator is holding the relay open.
 */
bool System_Get_RelayOverride(void)
{
    return s_webForceRelayOff;
}

/**
 * @brief  Allow or stop transmission of the 0x100 frame.
 * @param  on  false stops it, which lets the slaves fall asleep.
 * @return None.
 */
void System_Set_SlavesActive(bool on)
{
    s_webSlavesActive = on;
}

/**
 * @brief  Read whether the 0x100 frame is being transmitted.
 * @return true while the slaves are being kept awake.
 */
bool System_Get_SlavesActive(void)
{
    return s_webSlavesActive;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
