#ifndef BMS_RECEIVER_H
#define BMS_RECEIVER_H

#include <Arduino.h>
#include "driver/twai.h"

// --- CẤU HÌNH ---
#define CAN_TX_PIN      16   // Sửa theo chân thực tế
#define CAN_RX_PIN      17   // Sửa theo chân thực tế
#define BMS_SLAVE_ID    0x103

// Struct chứa dữ liệu BMS sau khi giải mã
typedef struct {
    float voltage;
    bool isConnected;
    uint32_t lastUpdate;
} BMS_Data_t;

// Khai báo hàm
void BMS_Init();
bool BMS_Read(BMS_Data_t *dataOut); // Trả về true nếu có gói tin mới

#endif