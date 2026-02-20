/*
 * BMS_ADC.c
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */

#include "BMS_ADC.h"

float BMS_ADC_GetVoltage(ADC_HandleTypeDef *hadc) {
    uint32_t adc_sum = 0;

    // --- TĂNG SỐ LẦN LẤY MẪU LÊN 100 ---

    for(int i = 0; i < NUM_SAMPLES; i++) {
        HAL_ADC_Start(hadc);
        if(HAL_ADC_PollForConversion(hadc, 1) == HAL_OK) { // Giảm timeout xuống 1ms cho nhanh
            adc_sum += HAL_ADC_GetValue(hadc);
        }
        HAL_ADC_Stop(hadc);
    }

    // Chia cho 100
    float adc_avg = (float)adc_sum / (float)NUM_SAMPLES;

    // Tính toán điện áp (Code cũ giữ nguyên)
    float v_pin = (adc_avg / ADC_RESOLUTION) * ADC_VREF;
    float v_bat = v_pin * ((ADC_R1_VAL + ADC_R2_VAL) / ADC_R2_VAL);

    return v_bat * ADC_CALIBRATION_FACTOR;
}
