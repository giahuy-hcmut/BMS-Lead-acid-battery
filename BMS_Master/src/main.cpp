#include <Arduino.h>
#include "System_Data.h"
#include "Task_CAN.h"
#include "Task_Logic.h"
#include "Task_Terminal.h" 
#include "Task_LCD.h" 
#include "Task_EspNow.h" // [ĐÃ THAY ĐỔI: Include Task ESP-NOW]
#include "Task_WebServer.h" // <--- THÊM DÒNG NÀY
#include "Task_Current.h"

void setup() {
    Serial.begin(115200);

    Serial.printf("\n\n>>> KÍCH THƯỚC GÓI TIN: %d Bytes <<<\n\n", sizeof(BMS_Telemetry_Packet));
    System_Data_Init();

    // --- CORE 1: NHIỆM VỤ SỐNG CÒN (REAL-TIME) ---
    // Task CAN (Priority 5)
    xTaskCreatePinnedToCore(Task_CAN_Run, "CAN", 4096, NULL, 5, NULL, 1);
    
    // Task Logic (Priority 4)
    xTaskCreatePinnedToCore(Task_Logic_Run, "Logic", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(Task_Current_Run, "Current", 4096, NULL, 4, NULL, 1);
    // --- CORE 0: NHIỆM VỤ HIỂN THỊ (GIAO TIẾP NGƯỜI DÙNG) ---
    // Task Terminal (Priority 1 - Thấp nhất)
    // Chạy ở Core 0 để không làm phiền Core 1 tính toán
    xTaskCreatePinnedToCore(Task_Terminal_Run, "Term", 4096, NULL, 1, NULL, 0);

    // <--- 2. Thêm Task LCD vào đây
    // Priority thấp (1), chạy Core 0
    //xTaskCreatePinnedToCore(Task_LCD_Run, "LCD", 4096, NULL, 1, NULL, 0);

    // <--- THÊM DÒNG NÀY (Để Stack Size là 8192 vì AsyncWeb cần nhiều RAM hơn một chút)
    xTaskCreatePinnedToCore(Task_WebServer_Run, "WebSrv", 8192, NULL, 2, NULL, 0);

       delay(500);

    // [ĐÃ THAY ĐỔI: Chạy Task phát sóng ESP-NOW ở Core 0]
    //xTaskCreatePinnedToCore(Task_EspNow_Run, "EspNowTx", 4096, NULL, 2, NULL, 0);
}

void loop() {
    // FreeRTOS quản lý các Task, hàm loop() bị vô hiệu hóa để giải phóng RAM
    vTaskDelete(NULL);
}