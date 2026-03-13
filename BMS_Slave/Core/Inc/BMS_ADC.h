/*
 * BMS_ADC.h
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */

#ifndef INC_BMS_ADC_H_
#define INC_BMS_ADC_H_

#include "stm32f1xx_hal.h" // Thay bằng f4/h7 tùy chip, ở đây là F1
#include "Shared_Data.h"

// --- CẤU HÌNH PHẦN CỨNG ---
// R1 nối V_BAT, R2 nối GND. Ví dụ R1=100k, R2=3.3k
#define ADC_VREF        3.3f
#define ADC_RESOLUTION  4095.0f
#define NUM_SAMPLES 100

#if (CAN_SLAVE_ID == 0x103)
	#define ADC_R1_VAL      46500.0f		//103
	#define ADC_R2_VAL      9750.0f			//103
	#define ADC_CALIBRATION_FACTOR 		0.99f //103

#elif (CAN_SLAVE_ID == 0x104)
	#define ADC_R1_VAL      45300.0f		//104
	#define ADC_R2_VAL      9680.0f		//104
	#define ADC_CALIBRATION_FACTOR 		1.03f//	104
#endif

// Hàm khởi tạo (nếu cần) và hàm đọc
float BMS_ADC_GetVoltage(ADC_HandleTypeDef *hadc);

#endif /* INC_BMS_ADC_H_ */
