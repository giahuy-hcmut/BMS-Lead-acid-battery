/**
 * @file      BMS_ADC.c
 * @author    Do Dang Gia Huy
 * @brief     Averaged ADC read, divider inversion and two-point calibration.
 * @date      2026-01-31
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Calibration procedure: build with CALIB_K = 1.0f and CALIB_B = 0.0f, watch
 * g_dbg_v_computed in STM32CubeIDE Live Expressions, record it against a
 * meter at two voltages, then compute K and B and write them into BMS_ADC.h
 * under the matching SLAVE_INDEX.
 */

/**********************
 *      INCLUDES
 **********************/

#include "BMS_ADC.h"

#include <stddef.h>         /* NULL */

/**********************
 *      DEFINES
 **********************/

/* Per-conversion poll timeout. One conversion takes about 7 us, so 2 ms is
 * three orders of magnitude of slack: it only expires if the peripheral is
 * genuinely stuck. */
#define ADC_POLL_TIMEOUT_MS     2U

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/* The ADC handle, owned by this driver. main() hands it over once via
 * BMS_ADC_Init(); after that no other file needs to know that an
 * ADC_HandleTypeDef even exists. */
static ADC_HandleTypeDef *s_hadc = NULL;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/* Debug aids, non-static so the debugger can resolve them. Declared in
 * BMS_ADC.h; see the note there. */
volatile float g_dbg_adc_avg    = 0.0f;
volatile float g_dbg_v_computed = 0.0f;

/**
 * @brief  Hand the ADC handle to the driver.
 * @param  hadc  Handle configured by MX_ADC1_Init().
 * @return None.
 */
void BMS_ADC_Init(ADC_HandleTypeDef *hadc)
{
    s_hadc = hadc;
}

/**
 * @brief  Measure the battery terminal voltage, calibration applied.
 * @return Terminal voltage in volts, or 0.0f when the driver was never
 *         initialised or no conversion completed.
 */
float BMS_ADC_GetVoltage(void)
{
    uint32_t adc_sum = 0U;
    uint32_t valid   = 0U;
    uint32_t i;
    float    adc_avg;
    float    v_pin;
    float    v_comp;

    /* BMS_ADC_Init() was never called. Returning 0.0 V is deliberate:
     * it is below THRESHOLD_UNDER_VOLT, so the under-voltage flag fires
     * and the master sees a faulty slave instead of a plausible reading. */
    if (s_hadc == NULL)
    {
        g_dbg_adc_avg    = 0.0f;
        g_dbg_v_computed = 0.0f;
        return 0.0f;
    }

    for (i = 0U; i < ADC_NUM_SAMPLES; i++)
    {
        HAL_ADC_Start(s_hadc);
        if (HAL_ADC_PollForConversion(s_hadc, ADC_POLL_TIMEOUT_MS) == HAL_OK)
        {
            adc_sum += HAL_ADC_GetValue(s_hadc);
            valid++;
        }
        HAL_ADC_Stop(s_hadc);
    }

    /* Divide by the samples that actually converted, not by the number
     * requested. A timed-out conversion contributes nothing to adc_sum, so
     * dividing by ADC_NUM_SAMPLES counted it as 0 V: losing 10 of 100 samples
     * silently reported 10% less voltage, with no fault bit raised, and a low
     * reading is exactly what makes the under-voltage protection trip. */
    if (valid == 0U)
    {
        /* Nothing converted at all. Report 0.0f, the same contract this
         * driver already uses for an uninitialised handle: 0 V sits below
         * THRESHOLD_UNDER_VOLT, so the master sees a faulty slave instead of
         * a plausible number. */
        g_dbg_adc_avg    = 0.0f;
        g_dbg_v_computed = 0.0f;
        return 0.0f;
    }

    adc_avg = (float)adc_sum / (float)valid;
    v_pin   = (adc_avg / ADC_RESOLUTION) * ADC_VREF;
    v_comp  = v_pin * ((ADC_R1_VAL + ADC_R2_VAL) / ADC_R2_VAL);

    /* Expose the raw values for calibration debug. */
    g_dbg_adc_avg    = adc_avg;
    g_dbg_v_computed = v_comp;

    return (CALIB_K * v_comp) + CALIB_B;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
