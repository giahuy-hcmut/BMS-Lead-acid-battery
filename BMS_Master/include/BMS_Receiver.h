#ifndef BMS_RECEIVER_H
#define BMS_RECEIVER_H

#include <Arduino.h>
#include "driver/twai.h"

// --- CẤU HÌNH CHÂN CAN (Sửa lại nếu bạn dùng chân khác) ---
#define CAN_TX_PIN      16
#define CAN_RX_PIN      17

// Struct dùng chung: Driver đóng gói -> Queue vận chuyển -> Task xử lý
typedef struct {
    uint32_t can_id;      // ID người gửi (0x103, 0x104...)
    float voltage;        // Điện áp đã giải mã
    uint32_t timestamp;   // Thời gian nhận
} BMS_Message_t;

// Khai báo hàm
void BMS_Driver_Init();
bool BMS_Driver_Read(BMS_Message_t *msgOut);

#endif