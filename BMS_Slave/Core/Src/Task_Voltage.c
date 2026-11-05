/**
 * @file      Task_Voltage.c
 * @author    Do Dang Gia Huy
 * @brief     Voltage acquisition with internal-resistance compensated UVP.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_Voltage.h"

#include "BMS_ADC.h"
#include "BMS_CAN.h"        /* BMS_CAN_GetLastRxTick - is the current fresh? */
#include "Board_Config.h"   /* THRESHOLD_UNDER_VOLT / THRESHOLD_OVER_VOLT   */
#include "Debug_Pins.h"
#include "SOC_Kalman.h"     /* KF_R0_NOM - ONE source for the 13.5 mOhm      */
#include "Shared_Data.h"

#include "main.h"           /* HAL_GetTick */

/**********************
 *      DEFINES
 **********************/

/* A current sample older than this is NOT used for compensation. Same value
 * as CURRENT_TIMEOUT_MS in Task_SOC: the master sends 0x100 every 5 ms, so
 * 100 ms means 20 consecutive frames were lost. */
#define UVP_CURRENT_TIMEOUT_MS   100U

/* Ceiling on the compensation term. In the far direction the term RAISES
 * v_uvp, so a garbage current reading could MASK a real under-voltage, and
 * that is the dangerous direction. 150 A * 13.5 mOhm is about 2.03 V, which
 * already covers the 100 A system (4 motors x 25 A) with margin. */
#define UVP_MAX_COMP_V           2.03f

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
 * @brief  Measure the terminal voltage, run the voltage checks and publish.
 * @return None.
 */
void Task_Voltage_Run(void)
{
    BMS_Snapshot_t snap;
    float          v_raw;
    float          v_uvp;
    float          comp;
    uint8_t        faults = 0U;

    DBG_SET(DBG_VOLT);

    /* 1. Measure. The driver already averages 100 samples and calibrates. */
    v_raw = BMS_ADC_GetVoltage();

    /* 2. UVP compares against the voltage COMPENSATED for internal
     *    resistance, not the raw reading.
     *
     * THRESHOLD_UNDER_VOLT = 10.50 V is a RESTING voltage (1.75 V/cell, from
     * the datasheet). Under load the terminal voltage sags by I*R0 = 1.35 V
     * at 100 A, so a battery resting at 11.85 V (about 17% SOC and still
     * usable) reads exactly 10.50 V and would trip for nothing.
     *
     * Compensate only while the current is FRESH. A stale or missing current
     * compensates wrongly, and in the far direction that MASKS a real
     * under-voltage; not compensating at all only trips EARLIER, which is
     * safe, so that is the default when the current is lost.
     *
     * The polarisation term V_RC is deliberately NOT compensated, only the
     * instantaneous R0, so v_uvp stays slightly below the true OCV during a
     * long discharge - again erring towards safety.
     *
     * R0 is NOT temperature compensated either: in the cold the real R0 is
     * LARGER, so the correction is smaller than reality and the trip comes
     * earlier. Safe, and simpler. */
    BMS_Data_GetSnapshot(&snap);

    v_uvp = v_raw;      /* default: raw voltage, no compensation */

    if ((HAL_GetTick() - BMS_CAN_GetLastRxTick()) <= UVP_CURRENT_TIMEOUT_MS)
    {
        comp = snap.current_a * KF_R0_NOM;      /* I > 0 = discharge -> add */

        if (comp > UVP_MAX_COMP_V)
        {
            comp = UVP_MAX_COMP_V;
        }

        if (comp < -UVP_MAX_COMP_V)
        {
            comp = -UVP_MAX_COMP_V;
        }

        v_uvp = v_raw + comp;
    }

    /* `faults` is a local that starts at 0 on every run, so there is no need
     * for the `else` branches that used to clear the bits.
     * OVP uses the RAW voltage: 15.00 V is a limit on the TERMINAL voltage
     * itself, whatever current the sag or rise comes from. */
    if (v_uvp < THRESHOLD_UNDER_VOLT)
    {
        faults |= ERROR_UNDER_VOLT;
    }

    if (v_raw > THRESHOLD_OVER_VOLT)
    {
        faults |= ERROR_OVER_VOLT;
    }

    /* 3. Publish through the store API. This task owns those two bits only
     *    and cannot touch another producer's temperature, current or SOC.
     *    It reports v_raw, the MEASURED voltage, not v_uvp: the web page and
     *    the master must see the real reading, and only the fault decision
     *    uses the compensated one. */
    BMS_Data_SetVoltage(v_raw, faults);

    DBG_CLR(DBG_VOLT);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
