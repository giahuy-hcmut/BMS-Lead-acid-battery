/**
 * @file      Task_SOC.c
 * @author    Do Dang Gia Huy
 * @brief     Filter lifecycle, measured time step and staleness handling.
 * @date      2026-08-06
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * See Task_SOC.h for the multirate contract.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_SOC.h"

#include "BMS_CAN.h"
#include "Debug_Pins.h"
#include "SOC_Kalman.h"
#include "Shared_Data.h"

#include "main.h"           /* HAL_GetTick */

/**********************
 *      DEFINES
 **********************/

/* No master frame for this long -> force the current to 0.
 * The master sends every 5 ms, so 100 ms means 20 consecutive frames were
 * lost. Integrating 100 ms of 100 A costs 100 * 0.1 / 72000 = 0.014% of SOC,
 * so nothing is lost by waiting that long before giving up. */
#define CURRENT_TIMEOUT_MS      100U

/** Milliseconds to seconds. */
#define MS_TO_S                 0.001f

/** SOC is published as whole percent; 0.5f rounds instead of truncating. */
#define SOC_PCT_SCALE           100.0f
#define SOC_PCT_ROUND           0.5f

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static KF_State_t s_kf;
static uint8_t    s_initialised = 0U;
static uint32_t   s_last_tick   = 0U;   /* HAL tick of the previous Predict */

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Run one Predict step, and an Update when a fresh voltage arrived.
 * @return None.
 */
void Task_SOC_Run(void)
{
    BMS_Snapshot_t snap;
    uint32_t       now = HAL_GetTick();
    float          dt;

    DBG_SET(DBG_SOC);

    BMS_Data_GetSnapshot(&snap);

    /* Init needs a REAL resting voltage to invert the OCV curve. Waiting for
     * the fresh-voltage flag proves Task_Voltage has produced at least one
     * ADC reading, so no arbitrary voltage threshold is needed here. */
    if (s_initialised == 0U)
    {
        if (BMS_Data_TakeVoltageFresh() != 0U)
        {
            SOC_Kalman_Init(&s_kf, snap.voltage_v);
            s_last_tick   = now;
            s_initialised = 1U;
        }

        DBG_CLR(DBG_SOC);       /* early exit: not initialised yet */
        return;
    }

    /* dt is MEASURED, never assumed to equal TASK_SOC_PERIOD_MS.
     *
     * No activation is lost: SCH_Update() raises run_me for the WHOLE group
     * of entries due on the same tick, and main() calls SCH_Dispatch() in a
     * tight loop, so every task that came due still runs within that tick.
     * What does vary is WHEN inside the tick this task is released. Priority
     * 3 puts it first on a shared deadline, but a task already running must
     * finish: Task_Temperature blocks about 4.9 ms bit-banging 1-Wire, which
     * is a whole period of this task.
     *
     * Coulomb integration only cares about the SUM of dt*I, and since
     * s_last_tick is advanced to `now` on every call, the dt values add up to
     * the true elapsed time regardless of that jitter. The 1 ms tick
     * resolution quantises each step but does not bias the sum. A hard-coded
     * 5 ms, by contrast, would turn every late release into a permanent
     * undercount.
     *
     * uint32_t subtraction is also correct across the ~49.7 day tick wrap. */
    dt = (float)(now - s_last_tick) * MS_TO_S;
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
     * real design bug once - see section 2 of the project brief. */
    if (BMS_Data_TakeVoltageFresh() != 0U)
    {
        (void)SOC_Kalman_Update(&s_kf, snap.voltage_v, snap.current_a, snap.temp_c);
    }

    /* Always through GetSOC(): the internal state is allowed to overshoot
     * [0,1] by the soft-clamp margin, the reported value is not. */
    BMS_Data_SetSoc((uint8_t)((SOC_Kalman_GetSOC(&s_kf) * SOC_PCT_SCALE) + SOC_PCT_ROUND));

    DBG_CLR(DBG_SOC);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
