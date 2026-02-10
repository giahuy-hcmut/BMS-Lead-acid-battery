#include "System_Manager.h"

// --- 1. KHỞI TẠO BIẾN THỰC SỰ ---
QueueHandle_t canQueue;
SemaphoreHandle_t dataMutex;
BMS_Pack_State globalPacks[4]; // Quản lý tối đa 4 pack (ID 0x103 -> 0x106)

// --- 2. CÁC TASK ---

// Task 1: Nhận CAN (Chạy Core 1 - Ưu tiên cao - Producer)
// Nhiệm vụ: Chỉ nhận và ném vào Queue, không xử lý gì thêm.
void Task_CAN_Rx(void *pvParameters) {
    BMS_Driver_Init(); 
    BMS_Message_t tempMsg;

    while (1) {
        if (BMS_Driver_Read(&tempMsg)) {
            xQueueSend(canQueue, &tempMsg, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Nhường CPU
    }
}

// Task 2: Xử lý & Hiển thị (Chạy Core 0 - Consumer)
// Nhiệm vụ: Cập nhật dữ liệu và hiển thị bảng thông số mỗi giây.
void Task_Process(void *pvParameters) {
    BMS_Message_t rxMsg;
    uint32_t lastPrintTime = 0;
    
    while (1) {
        // A. NHẬN DỮ LIỆU (Timeout 10ms để còn kịp chạy xuống phần hiển thị)
        if (xQueueReceive(canQueue, &rxMsg, pdMS_TO_TICKS(10)) == pdTRUE) {
            
            // Dùng Mutex bảo vệ mảng toàn cục
            if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
                
                // Mapping ID: 0x103 -> Index 0, 0x104 -> Index 1
                int idx = rxMsg.can_id - 0x103;
                
                // Chỉ chấp nhận các ID hợp lệ từ 0x103 đến 0x106
                if (idx >= 0 && idx < 4) {
                    globalPacks[idx].voltage = rxMsg.voltage;
                    globalPacks[idx].lastUpdate = rxMsg.timestamp;
                    globalPacks[idx].isConnected = true;
                }
                xSemaphoreGive(dataMutex);
            }
        }

        // B. HIỂN THỊ DẠNG BẢNG (1 giây update 1 lần)
        if (millis() - lastPrintTime > 1000) {
            lastPrintTime = millis();

            // In dòng trống để tạo cảm giác làm mới màn hình
            Serial.println("\n\n---------------------------------");
            Serial.println("   GIAM SAT DIEN AP PIN (V1.0)   ");
            Serial.println("---------------------------------");

            // Mượn chìa khóa để đọc dữ liệu ra hiển thị
            if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
                
                // Vòng lặp duyệt qua các Pack (Hiện tại test 2 pack: 0x103, 0x104)
                for (int i = 0; i < 2; i++) { 
                    Serial.print("| PACK ");
                    Serial.print(i + 1);
                    Serial.print(" (0x");
                    Serial.print(0x103 + i, HEX);
                    Serial.print(") | ");

                    // Kiểm tra trạng thái Online/Offline
                    // Nếu quá 3000ms (3s) mà không thấy cập nhật -> Coi như mất kết nối
                    if (globalPacks[i].lastUpdate > 0 && (millis() - globalPacks[i].lastUpdate < 3000)) {
                        Serial.print(globalPacks[i].voltage, 2); // In 2 số lẻ
                        Serial.println(" V   |   OK   ✅");
                    } else {
                        Serial.println("--.-- V   |   LOST ❌");
                    }
                }
                xSemaphoreGive(dataMutex);
            }
            Serial.println("---------------------------------");
        }
    }
}

// --- 3. HÀM INIT ---
void System_Init() {
    Serial.println("[SYSTEM] Init FreeRTOS...");

    canQueue = xQueueCreate(20, sizeof(BMS_Message_t));
    dataMutex = xSemaphoreCreateMutex();

    // Task CAN (Core 1)
    xTaskCreatePinnedToCore(Task_CAN_Rx, "CAN_Rx", 4096, NULL, 2, NULL, 1);
    
    // Task Process (Core 0)
    xTaskCreatePinnedToCore(Task_Process, "Process", 4096, NULL, 1, NULL, 0);
    
    Serial.println("[SYSTEM] Started Dashboard Mode");
}