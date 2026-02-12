#include <Arduino.h>
#include "System_Data.h"
#include "Task_CAN.h"
#include "Task_Logic.h"
#include "Task_Terminal.h" // <--- Include module mới
#include "Task_LCD.h" // <--- 1. Thêm dòng này

void setup() {
    Serial.begin(115200);
    System_Data_Init();

    // --- CORE 1: NHIỆM VỤ SỐNG CÒN (REAL-TIME) ---
    // Task CAN (Priority 5)
    xTaskCreatePinnedToCore(Task_CAN_Run, "CAN", 4096, NULL, 5, NULL, 1);
    
    // Task Logic (Priority 4)
    xTaskCreatePinnedToCore(Task_Logic_Run, "Logic", 4096, NULL, 4, NULL, 1);

    // --- CORE 0: NHIỆM VỤ HIỂN THỊ (GIAO TIẾP NGƯỜI DÙNG) ---
    // Task Terminal (Priority 1 - Thấp nhất)
    // Chạy ở Core 0 để không làm phiền Core 1 tính toán
    xTaskCreatePinnedToCore(Task_Terminal_Run, "Term", 4096, NULL, 1, NULL, 0);

    // <--- 2. Thêm Task LCD vào đây
    // Priority thấp (1), chạy Core 0. Stack 4096 cho an toàn.
    xTaskCreatePinnedToCore(Task_LCD_Run, "LCD", 4096, NULL, 1, NULL, 0);

    Serial.println(">>> SYSTEM STARTED <<<");
}

void loop() {
    vTaskDelete(NULL);
}