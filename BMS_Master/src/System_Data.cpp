#include "System_Data.h"

// 1. Biến toàn cục thực tế
BMS_Pack_State globalPacks[TOTAL_PACKS]; 
SemaphoreHandle_t dataMutex;   
QueueHandle_t canQueue;        

// 2. Khởi tạo
void System_Data_Init() {
    dataMutex = xSemaphoreCreateMutex();
    canQueue = xQueueCreate(20, sizeof(BMS_Message_t));
    
    // Xóa sạch dữ liệu ban đầu
    for(int i=0; i<TOTAL_PACKS; i++) {
        globalPacks[i].voltage = 0;
        globalPacks[i].current = 0;
        globalPacks[i].soc = 0;
        globalPacks[i].lastUpdate = 0;
        globalPacks[i].isConnected = false;
    }
}

// --- HÀM GHI AN TOÀN (Dành cho Task Logic) ---
void System_Update_Pack(uint32_t can_id, float voltage) {
    int idx = can_id - CAN_BASE_ID;
    
    // Safety Check: Kiểm tra chỉ số mảng
    if (idx < 0 || idx >= TOTAL_PACKS) return;

    // Vào khóa -> Ghi -> Ra ngay
    if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
        globalPacks[idx].voltage = voltage;
        globalPacks[idx].lastUpdate = millis(); // Cập nhật thời gian thực
        xSemaphoreGive(dataMutex);
    }
}

// --- HÀM ĐỌC AN TOÀN (Dành cho Task LCD/Terminal) ---
void System_Get_Snapshot(BMS_Pack_State* buffer) {
    // Vào khóa -> Copy -> Ra ngay
    if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
        for(int i=0; i<TOTAL_PACKS; i++) {
            buffer[i] = globalPacks[i]; // Copy từng phần tử sang bộ đệm riêng
        }
        xSemaphoreGive(dataMutex);
    }
}