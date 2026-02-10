/*
 * Shared_Data.h
 *
 *  Created on: Feb 10, 2026
 *      Author: User
 */

#ifndef INC_SHARED_DATA_H_
#define INC_SHARED_DATA_H_

#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdint.h>

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

#endif

#endif /* INC_SHARED_DATA_H_ */
