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
// Cầu phân áp: R1 nối V_BAT, R2 nối GND. Đọc áp tại nút giữa vào PB1 (ADC1_IN9).
//   V_pin  = V_BAT * R2/(R1+R2)
//   V_tinh = V_pin * (R1+R2)/R2   (đảo lại ra áp bình danh nghĩa)
// Chuẩn hóa 33k/9.1k GIỐNG cả 5 slave (tỉ lệ 0.2162: 14V -> 3.03V < 3.3V an toàn;
// R_source = 33k//9.1k = 7.13k, hợp ADC với sample time 71.5 cycles).
#define ADC_VREF        3.3f
#define ADC_RESOLUTION  4095.0f
#define NUM_SAMPLES     100

#define ADC_R1_VAL      33000.0f    // Ohm - nối V_BAT
#define ADC_R2_VAL      9100.0f     // Ohm - nối GND

// --- CALIB 2 ĐIỂM (per-slave): V_that = CALIB_K * V_tinh + CALIB_B ---
// Sửa cả DỐC (K) lẫn OFFSET (B) — chính xác hơn hệ số nhân đơn.
// Đo 2 mốc bằng VOM, tính:  K = (V2-V1)/(Vt2-Vt1),  B = V1 - K*Vt1.
// Giá trị 1.0 / 0.0 = CHƯA calib (dùng để CHẠY calib), điền số đo sau.
#if (CAN_SLAVE_ID == 0x103)
    #define CALIB_K     0.971f  //103 đo 2 mốc: (8.95->9.16),(11.70->11.83)
    #define CALIB_B     0.470f  //103
#elif (CAN_SLAVE_ID == 0x104)
    #define CALIB_K     1.0f    //104 TODO đo
    #define CALIB_B     0.0f    //104 TODO đo
#elif (CAN_SLAVE_ID == 0x105)
    #define CALIB_K     1.0f    //105 TODO đo
    #define CALIB_B     0.0f    //105 TODO đo
#elif (CAN_SLAVE_ID == 0x106)
    #define CALIB_K     1.0f    //106 TODO đo
    #define CALIB_B     0.0f    //106 TODO đo
#elif (CAN_SLAVE_ID == 0x107)
    #define CALIB_K     1.0f    //107 TODO đo
    #define CALIB_B     0.0f    //107 TODO đo
#endif

// --- Biến DEBUG (global, cho Live Expressions lúc calib) ---
extern volatile float g_dbg_adc_avg;    // ADC trung bình 100 mẫu (0..4095)
extern volatile float g_dbg_v_computed; // áp bình trước calib (V)

// Hàm đọc áp bình (đã calib). Trả về V.
float BMS_ADC_GetVoltage(ADC_HandleTypeDef *hadc);

#endif /* INC_BMS_ADC_H_ */
