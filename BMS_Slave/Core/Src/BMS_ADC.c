/*
 * BMS_ADC.c
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */

#include "BMS_ADC.h"
#include <stddef.h>          /* NULL */

/* Debug globals — visible in STM32CubeIDE "Live Expressions" during
 * calibration. Kept non-static so the debugger can resolve them. */
volatile float g_dbg_adc_avg    = 0.0f;
volatile float g_dbg_v_computed = 0.0f;

/* The ADC handle, owned by this driver. main() hands it over once via
 * BMS_ADC_Init(); after that no other file needs to know that an
 * ADC_HandleTypeDef even exists. */
static ADC_HandleTypeDef *s_hadc = NULL;

void BMS_ADC_Init(ADC_HandleTypeDef *hadc)
{
    s_hadc = hadc;
}

/*
 * Read the battery terminal voltage.
 *   1. Average NUM_SAMPLES ADC conversions (reduce noise).
 *   2. Convert to pin voltage, then invert the divider -> V_computed.
 *   3. Apply the 2-point calibration -> true voltage.
 *
 * CALIBRATION: build with CALIB_K=1, CALIB_B=0, watch g_dbg_v_computed in
 * Live Expressions, record it against a VOM at two voltages, then compute
 * K,B and put them in BMS_ADC.h (per slave).
 */
float BMS_ADC_GetVoltage(void)
{
    uint32_t adc_sum = 0;

    /* BMS_ADC_Init() was never called. Returning 0.0 V is deliberate:
     * it is below THRESHOLD_UNDER_VOLT, so the under-voltage flag fires
     * and the master sees a faulty slave instead of a plausible reading. */
    if (s_hadc == NULL) {
        g_dbg_adc_avg    = 0.0f;
        g_dbg_v_computed = 0.0f;
        return 0.0f;
    }

    for (int i = 0; i < NUM_SAMPLES; i++) {
        HAL_ADC_Start(s_hadc);
        if (HAL_ADC_PollForConversion(s_hadc, 2) == HAL_OK) {   /* 2 ms timeout */
            adc_sum += HAL_ADC_GetValue(s_hadc);
        }
        HAL_ADC_Stop(s_hadc);
    }

    float adc_avg   = (float)adc_sum / (float)NUM_SAMPLES;
    float v_pin     = (adc_avg / ADC_RESOLUTION) * ADC_VREF;
    float v_comp    = v_pin * ((ADC_R1_VAL + ADC_R2_VAL) / ADC_R2_VAL);

    /* expose raw values for calibration debug */
    g_dbg_adc_avg    = adc_avg;
    g_dbg_v_computed = v_comp;

    /* 2-point calibration (identity K=1,B=0 until measured) */
    return CALIB_K * v_comp + CALIB_B;
}
