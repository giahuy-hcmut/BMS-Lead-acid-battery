#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// Cấu trúc dữ liệu của 1 Pack Pin (Lưu trong kho)
struct BMS_Pack_State {
    float voltage;
    float current;
    int soc;
    uint32_t lastUpdate;
    bool isConnected;
};

// Cấu trúc tin nhắn CAN (Truyền trên băng chuyền Queue)
typedef struct {
    uint32_t can_id;
    float voltage;
    // float current; // (Sau này mở rộng thêm ở đây)
    // int soc;
} BMS_Message_t;

// Khai báo extern để các file khác nhìn thấy
extern BMS_Pack_State globalPacks[5]; 
extern SemaphoreHandle_t dataMutex;
extern QueueHandle_t canQueue;

void System_Data_Init();

#endif