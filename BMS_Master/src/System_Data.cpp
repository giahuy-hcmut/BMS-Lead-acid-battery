#include "System_Data.h"

// 1. Biến toàn cục (Global Variables)
BMS_Pack_State globalPacks[5]; // Lưu trạng thái 5 pack pin
SemaphoreHandle_t dataMutex;   // Chìa khóa kho
QueueHandle_t canQueue;        // Băng chuyền tin nhắn

// 2. Hàm khởi tạo
void System_Data_Init() {
    // Tạo Mutex (Khóa bảo vệ)
    dataMutex = xSemaphoreCreateMutex();
    
    // Tạo Queue (Hàng đợi chứa tối đa 20 tin nhắn)
    canQueue = xQueueCreate(20, sizeof(BMS_Message_t));
}