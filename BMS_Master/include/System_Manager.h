#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include "BMS_Receiver.h" // Để hiểu struct BMS_Message_t

// --- DỮ LIỆU TOÀN CỤC (GLOBAL SHARED) ---
// Struct lưu trạng thái từng bình
struct BMS_Pack_State {
    float voltage;
    uint32_t lastUpdate;
    bool isConnected;
};

// Khai báo extern (Báo cho các file khác biết biến này có tồn tại)
extern BMS_Pack_State globalPacks[4]; 
extern SemaphoreHandle_t dataMutex;

// Hàm khởi động hệ thống RTOS
void System_Init();

#endif