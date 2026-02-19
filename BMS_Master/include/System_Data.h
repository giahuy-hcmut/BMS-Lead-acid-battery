#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// ==========================================
// [1. CẤU HÌNH HỆ THỐNG - SYSTEM CONFIG]
// ==========================================
#define TOTAL_PACKS     5       // Số lượng Pack Pin
#define CAN_BASE_ID     0x103   // ID bắt đầu
#define CAN_BAUD_RATE   500000  // Tốc độ CAN
#define LCD_ADDR        0x27    // Địa chỉ I2C
#define LCD_TIMEOUT     3000    // Timeout báo mất kết nối (ms)

// ==========================================
// [2. SƠ ĐỒ CHÂN PHẦN CỨNG - PIN MAPPING]
// ==========================================
#define PIN_CAN_TX      GPIO_NUM_16
#define PIN_CAN_RX      GPIO_NUM_17
#define PIN_I2C_SDA     21
#define PIN_I2C_SCL     22

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

// Biến toàn cục (Chỉ khai báo extern, không dùng trực tiếp ở các Task)
extern BMS_Pack_State globalPacks[TOTAL_PACKS]; 
extern QueueHandle_t canQueue;

// Hàm khởi tạo
void System_Data_Init();

// ==========================================
// [4. THREAD-SAFE API (CỬA CHÍNH BẢO VỆ)]
// ==========================================
// Các Task sẽ gọi 2 hàm này thay vì dùng xSemaphoreTake trực tiếp
void System_Update_Pack(uint32_t can_id, float voltage);
void System_Get_Snapshot(BMS_Pack_State* buffer);

#endif