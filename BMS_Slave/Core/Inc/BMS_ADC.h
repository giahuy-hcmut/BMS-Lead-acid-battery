/*
 * BMS_ADC.h
 *
 *  Created on: Jan 31, 2026
 *      Author: User
 */

#ifndef INC_BMS_ADC_H_
#define INC_BMS_ADC_H_

#include "stm32f1xx_hal.h" // Thay bằng f4/h7 tùy chip, ở đây là F1
#include "Board_Config.h"  // chi can SLAVE_INDEX. KHONG include Shared_Data.h:
                           // driver khong duoc phu thuoc tang du lieu.

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
#if   (SLAVE_INDEX == 0U)   /* CAN 0x103 */
    #define CALIB_K     0.971f  // đo 2 mốc: (8.95->9.16),(11.70->11.83)
    #define CALIB_B     0.470f
#elif (SLAVE_INDEX == 1U)   /* CAN 0x104 */
    #define CALIB_K     1.0f    // TODO đo
    #define CALIB_B     0.0f    // TODO đo
#elif (SLAVE_INDEX == 2U)   /* CAN 0x105 */
    #define CALIB_K     1.0f    // TODO đo
    #define CALIB_B     0.0f    // TODO đo
#elif (SLAVE_INDEX == 3U)   /* CAN 0x106 */
    #define CALIB_K     1.0f    // TODO đo
    #define CALIB_B     0.0f    // TODO đo
#elif (SLAVE_INDEX == 4U)   /* CAN 0x107 */
    #define CALIB_K     1.0f    // TODO đo
    #define CALIB_B     0.0f    // TODO đo
#else
    #error "No calibration constants for this SLAVE_INDEX"
#endif

// --- Biến DEBUG (global, cho Live Expressions lúc calib) ---
// Ngoai le co y thuc cua quy tac dong goi: chung phai global de debugger
// giai duoc ten. Chi doc, khong tham gia logic nao.
extern volatile float g_dbg_adc_avg;    // ADC trung bình 100 mẫu (0..4095)
extern volatile float g_dbg_v_computed; // áp bình trước calib (V)

// Nộp handle ADC MỘT LẦN lúc khởi động. Phải gọi trước BMS_ADC_GetVoltage().
void BMS_ADC_Init(ADC_HandleTypeDef *hadc);

// Hàm đọc áp bình (đã calib). Trả về V.
// Không nhận tham số: driver tự giữ handle, nhờ đó tầng Task không phải
// biết đến kiểu dữ liệu nào của HAL.
float BMS_ADC_GetVoltage(void);

#endif /* INC_BMS_ADC_H_ */
