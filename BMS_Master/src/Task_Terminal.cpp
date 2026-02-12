#include "Task_Terminal.h"
#include "System_Data.h" // Để lấy dữ liệu từ kho chung

void Task_Terminal_Run(void *pvParameters) {
    while (1) {
        // 1. Chờ 1 giây (Refresh rate)
        // Việc in ấn không cần quá nhanh, 1s/lần là đẹp
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 2. In tiêu đề
        Serial.println("\n\n=================================");
        Serial.printf("   BMS MASTER DASHBOARD (Up: %lu s)\n", millis()/1000);
        Serial.println("=================================");
        Serial.println("| ID    | VOLTAGE | STATUS      |");
        Serial.println("|-------|---------|-------------|");

        // 3. Mượn chìa khóa kho để đọc dữ liệu (Thread-safe)
        // Chỉ mượn trong thời gian cực ngắn để copy dữ liệu ra hoặc in nhanh
        if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
            
            // Duyệt qua 2 Slave giả lập (0x103, 0x104)
            for (int i = 0; i < 2; i++) {
                // Kiểm tra Timeout (3 giây không có tin -> Mất kết nối)
                bool isOnline = (globalPacks[i].lastUpdate > 0) && 
                                (millis() - globalPacks[i].lastUpdate < 3000);
                
                // Cập nhật lại trạng thái kết nối vào kho (để Logic biết)
                globalPacks[i].isConnected = isOnline;

                // In ra dòng thông tin
                Serial.printf("| 0x%03X | ", 0x103 + i);
                
                if (isOnline) {
                    Serial.printf("%5.2f V | [ONLINE] ✅ |\n", globalPacks[i].voltage);
                } else {
                    Serial.printf(" --.-- V | [LOST]   ❌ |\n");
                }
            }

            // Trả chìa khóa ngay lập tức!
            xSemaphoreGive(dataMutex);
        } else {
            Serial.println("[SYSTEM] DATA BUSY!");
        }
        Serial.println("=================================");
    }
}