#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include "Config.h" 

// ==========================================
// [3. CẤU TRÚC DỮ LIỆU]
// ==========================================
struct BMS_Pack_State {
    float voltage;
    float current;
    int soc;
    int8_t temperature;    // Bổ sung: Lưu nhiệt độ thực tế (đã trừ 40)
    uint8_t status;        // Bổ sung: Lưu mã lỗi (0x00 là bình thường)
    uint32_t lastUpdate;
    bool isConnected;
};

typedef struct {
    uint32_t can_id;
    float voltage;
    int8_t temperature;    // <--- THÊM DÒNG NÀY (Sửa lỗi cho Task_CAN và Task_Logic)
    uint8_t status;        // <--- THÊM DÒNG NÀY (Sửa lỗi cho Task_CAN và Task_Logic)
} BMS_Message_t;

// [ĐÃ THAY ĐỔI: Đưa struct ESP-NOW vào đây, dùng Macro TOTAL_PACKS để tránh lỗi khi đổi số bình]
typedef struct __attribute__((packed)) {
    float totalVoltage;     // 4 bytes
    float systemCurrent;    // 4 bytes
    int   systemSOC;        // 4 bytes
    float packVolts[5];     // 20 bytes (5 packs)
    int8_t packTemps[5];    // 5 bytes
    uint8_t packStatus[5];  // 5 bytes
    bool  isOnline[5];      // 5 bytes
} BMS_Telemetry_Packet;     // Tổng cộng: 47 bytes

// Biến toàn cục (Chỉ khai báo extern, không dùng trực tiếp ở các Task)
extern BMS_Pack_State globalPacks[TOTAL_PACKS]; 
extern SemaphoreHandle_t dataMutex;   
extern QueueHandle_t canQueue;        

// API Hệ thống
void System_Data_Init();
// Sửa lại khai báo hàm ở cuối file
void System_Update_Pack(uint32_t can_id, float voltage, int8_t temp, uint8_t status);
void System_Update_Current(float current);
void System_Get_Snapshot(BMS_Pack_State *snapshotArray);

#endif