/*
 * Task_SOC.c
 *
 *  Created on: Aug 6, 2026
 *      Author: User
 *
 *  See Task_SOC.h for the multirate contract.
 */

#include "Task_SOC.h"
#include "SOC_Kalman.h"
#include "Shared_Data.h"
#include "BMS_CAN.h"
#include "main.h"            /* HAL_GetTick */
#include "Debug_Pins.h"

// Khong nhan frame CAN_MASTER_ID qua lau -> ep dong ve 0.
// Master phat moi 5 ms, nen 100 ms = 20 frame lien tiep bi mat moi kich.
// Sai so SOC neu tich phan them 100 ms o dong 100 A:
//   100 A * 0.1 s / 72000 C = 0.014%  -> khong dang ke.
#define CURRENT_TIMEOUT_MS   100U

static KF_State_t s_kf;
static uint8_t    s_initialised = 0;
static uint32_t   s_last_tick   = 0;   /* HAL tick of the previous Predict */

void Task_SOC_Run(void)
{
    BMS_Snapshot_t snap;
    uint32_t now = HAL_GetTick();
    float    dt;

    DBG_SET(DBG_SOC);           // bat den: task SOC bat dau chay

    BMS_Data_GetSnapshot(&snap);

    /* Init needs a REAL resting voltage to invert the OCV curve. Waiting for
     * the fresh-voltage flag proves Task_Voltage has produced at least one ADC
     * reading, so no arbitrary voltage threshold is needed here. */
    if (s_initialised == 0U)
    {
        if (BMS_Data_TakeVoltageFresh() != 0U)
        {
            SOC_Kalman_Init(&s_kf, snap.voltage_v);
            s_last_tick   = now;
            s_initialised = 1U;
        }
        DBG_CLR(DBG_SOC);       // tat den: loi ra som (chua init xong)
        return;
    }

    /* dt is MEASURED, never assumed to be the registered period.
     *
     * SCH_Update() raises RunMe for the head node only, so at most one task is
     * dispatched per tick. This task asks for every tick, which means it loses
     * one whenever another task comes due: 20 Task_Voltage + 1 Task_CAN +
     * 1 Task_Temperature per 200 ticks, so it actually runs ~89% of them. A
     * hard-coded 5 ms would make Coulomb integration undercount by ~11% - a
     * systematic error, and one SCH_GetOverrunCount() cannot see because the
     * re-armed instance always starts at RunMe = 0.
     *
     * Coulomb integration only cares about the SUM of dt*I, and since
     * s_last_tick is advanced to `now` every call, the dt values add up to the
     * true elapsed time regardless of jitter or skipped ticks. The 1 ms tick
     * resolution quantises each step but does not bias the sum.
     *
     * uint32_t subtraction is also correct across the ~49.7 day tick wrap. */
    dt = (float)(now - s_last_tick) * 0.001f;
    s_last_tick = now;

    /* Stale current: the master stopped talking. Holding the last value would
     * let Coulomb integration drift forever, so zero it and let the filter
     * lean on voltage alone. Written back to the store so bytes 2-3 of the
     * outgoing CAN frame stay consistent with what the filter actually used. */
    if ((now - BMS_CAN_GetLastRxTick()) > CURRENT_TIMEOUT_MS)
    {
        BMS_Data_SetCurrent(0.0f);
        snap.current_a = 0.0f;
    }

    SOC_Kalman_Predict(&s_kf, snap.current_a, dt);

    /* Update ONLY on a fresh voltage. Correcting with a stale sample was a
     * real design bug once - see PROJECT_BRIEF section 2. */
    if (BMS_Data_TakeVoltageFresh() != 0U)
    {
        SOC_Kalman_Update(&s_kf, snap.voltage_v, snap.current_a, snap.temp_c);
    }

    /* Always through GetSOC(): the internal state is allowed to overshoot
     * [0,1] by the soft-clamp margin, the reported value is not. */
    BMS_Data_SetSoc((uint8_t)((SOC_Kalman_GetSOC(&s_kf) * 100.0f) + 0.5f));
    DBG_CLR(DBG_SOC);           // tat den: task SOC ket thuc
}
