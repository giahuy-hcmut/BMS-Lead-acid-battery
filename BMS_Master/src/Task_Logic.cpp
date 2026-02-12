#include "Task_Logic.h"
#include "System_Data.h"

void Task_Logic_Run(void *pvParameters) {
    BMS_Message_t msg;

    while (1) {
        // 1. Chờ dữ liệu từ CAN (Block vô hạn)
        if (xQueueReceive(canQueue, &msg, portMAX_DELAY) == pdTRUE) {
            
            // 2. Cập nhật vào kho
            if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
                
                int idx = msg.can_id - 0x103;
                if (idx >= 0 && idx < 5) {
                    globalPacks[idx].voltage = msg.voltage;
                    
                    // Ghi nhận thời gian cập nhật để Task_Terminal tính Timeout
                    globalPacks[idx].lastUpdate = millis(); 
                }
                
                xSemaphoreGive(dataMutex);
            }
        }
    }
}