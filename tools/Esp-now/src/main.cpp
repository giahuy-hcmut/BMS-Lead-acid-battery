#include <Arduino.h>
#include "System_Data.h"
#include "Task_EspNowRx.h"
#include "Task_LCD.h"
#include "Task_Terminal.h"

void setup() {
    Serial.begin(115200);
    System_Data_Init();
    Serial.printf("\n\n>>> KÍCH THƯỚC GÓI TIN: %d Bytes <<<\n\n", sizeof(BMS_Telemetry_Packet));

    // --- CORE 1: Xử lý Giao tiếp Mạng ---
    // Task nhận sóng có độ ưu tiên cao nhất
    xTaskCreatePinnedToCore(Task_EspNowRx_Run, "EspNowRx", 4096, NULL, 5, NULL, 1);

    // --- CORE 0: Xử lý Giao diện ---
    xTaskCreatePinnedToCore(Task_LCD_Run, "LCD", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(Task_Terminal_Run, "Term", 4096, NULL, 1, NULL, 0);
}

void loop() {
    vTaskDelete(NULL);
}