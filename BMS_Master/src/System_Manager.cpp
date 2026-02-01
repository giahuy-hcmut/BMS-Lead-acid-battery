#include "System_Manager.h"

// --- 1. KHỞI TẠO BIẾN THỰC SỰ ---
QueueHandle_t canQueue;
SemaphoreHandle_t dataMutex;
BMS_Pack_State globalPacks[4]; // Mảng lưu trữ 4 bình (0..3)

// --- 2. CÁC TASK ---

// Task 1: Nhận CAN (Chạy Core 1 - Ưu tiên cao)
void Task_CAN_Rx(void *pvParameters) {
    BMS_Driver_Init(); // Gọi driver phần cứng
    BMS_Message_t tempMsg;

    while (1) {
        // Đọc liên tục, nếu có tin thì ném vào Queue
        if (BMS_Driver_Read(&tempMsg)) {
            xQueueSend(canQueue, &tempMsg, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Nhường CPU 1ms
    }
}

// Task 2: Xử lý & Hiển thị (Chạy Core 0 - Ưu tiên thấp hơn)
void Task_Process(void *pvParameters) {
    BMS_Message_t rxMsg;
    
    while (1) {
        // Chờ tin nhắn từ Queue (Block nếu rỗng)
        if (xQueueReceive(canQueue, &rxMsg, portMAX_DELAY) == pdTRUE) {
            
            // Dùng Mutex bảo vệ mảng toàn cục
            if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
                
                // Mapping ID: 0x103 -> Index 0
                int idx = rxMsg.can_id - 0x103;
                
                if (idx >= 0 && idx < 4) {
                    // Cập nhật dữ liệu
                    globalPacks[idx].voltage = rxMsg.voltage;
                    globalPacks[idx].lastUpdate = rxMsg.timestamp;
                    globalPacks[idx].isConnected = true;
                    
                    // --- IN RA SERIAL MONITOR ---
                    Serial.print("[SYS] Core ");
                    Serial.print(xPortGetCoreID());
                    Serial.print(" | ID: 0x");
                    Serial.print(rxMsg.can_id, HEX);
                    Serial.print(" | Vol: ");
                    Serial.println(rxMsg.voltage);
                }
                
                xSemaphoreGive(dataMutex); // Trả chìa khóa
            }
        }
    }
}

// --- 3. HÀM INIT ---
void System_Init() {
    Serial.println("[SYSTEM] Init RTOS...");

    canQueue = xQueueCreate(20, sizeof(BMS_Message_t));
    dataMutex = xSemaphoreCreateMutex();

    // Tạo Task CAN (Core 1)
    xTaskCreatePinnedToCore(Task_CAN_Rx, "CAN_Rx", 4096, NULL, 2, NULL, 1);
    
    // Tạo Task Process (Core 0)
    xTaskCreatePinnedToCore(Task_Process, "Process", 4096, NULL, 1, NULL, 0);
    
    Serial.println("[SYSTEM] Started");
}