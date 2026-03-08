/*
 * Shared_Data.h
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */


#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdint.h>

// ==========================================
// ⚙️ CẤU HÌNH HỆ THỐNG (SYSTEM CONFIG)
// ==========================================


// 1. CẤU HÌNH BẢO VỆ PIN (Dành cho Li-ion 4.2V Testbench)
#define THRESHOLD_OVER_VOLT     12.7f   // Quá áp (V)
#define THRESHOLD_UNDER_VOLT    9.00f   // Sụt áp (V)
#define THRESHOLD_OVER_TEMP     60.0f   // Quá nhiệt độ (C)

// 2. CẤU HÌNH MẠNG LƯỚI
#define CAN_SLAVE_ID            0x103   // Địa chỉ CAN của Node này

// Định nghĩa các cờ lỗi (Bitmask)
#define ERROR_NONE          0x00
#define ERROR_OVER_VOLT     0x01
#define ERROR_UNDER_VOLT    0x02
#define ERROR_OVER_TEMP     0x04

typedef struct {
    float voltage;      // Điện áp (V)
    float current;      // Dòng điện (A) - Nếu có
    float temp;         // Nhiệt độ (doC)
    uint8_t soc;        // Dung lượng (%)
    uint8_t status;     // Trạng thái lỗi
} BMS_State_t;

// Khai báo biến extern để các file khác dùng chung
extern BMS_State_t myBMS;



#endif /* SHARED_DATA_H */
