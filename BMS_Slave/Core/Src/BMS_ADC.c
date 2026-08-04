/*
 * BMS_ADC.c
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */

#include "BMS_ADC.h"

/* Debug globals — visible in STM32CubeIDE "Live Expressions" during
 * calibration. Kept non-static so the debugger can resolve them. */
volatile float g_dbg_adc_avg    = 0.0f;
volatile float g_dbg_v_computed = 0.0f;

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
float BMS_ADC_GetVoltage(ADC_HandleTypeDef *hadc)
{
    uint32_t adc_sum = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        HAL_ADC_Start(hadc);
        if (HAL_ADC_PollForConversion(hadc, 2) == HAL_OK) {   /* 2 ms timeout */
            adc_sum += HAL_ADC_GetValue(hadc);
        }
        HAL_ADC_Stop(hadc);
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
