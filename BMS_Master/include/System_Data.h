#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include "Config.h" // [ĐÃ THAY ĐỔI: Kéo toàn bộ cấu hình vào đây]

// ==========================================
// [3. CẤU TRÚC DỮ LIỆU]
// ==========================================
struct BMS_Pack_State {
    float voltage;
    float current;
    int soc;
    uint32_t lastUpdate;
    bool isConnected;
};

typedef struct {
    uint32_t can_id;
    float voltage;
    // float current; // Mở rộng sau này
} BMS_Message_t;

// [ĐÃ THAY ĐỔI: Đưa struct ESP-NOW vào đây, dùng Macro TOTAL_PACKS để tránh lỗi khi đổi số bình]
typedef struct __attribute__((packed)) {
    float totalVoltage;
    float systemCurrent;
    float packVolts[TOTAL_PACKS];
    bool  isOnline[TOTAL_PACKS];
} BMS_Telemetry_Packet;

// Biến toàn cục (Chỉ khai báo extern, không dùng trực tiếp ở các Task)
extern BMS_Pack_State globalPacks[TOTAL_PACKS]; 
extern SemaphoreHandle_t dataMutex;   
extern QueueHandle_t canQueue;        

// API Hệ thống
void System_Data_Init();
void System_Update_Pack(uint32_t can_id, float voltage);
void System_Update_Current(float current);
void System_Get_Snapshot(BMS_Pack_State *snapshotArray);

#endif