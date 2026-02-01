#include <Arduino.h>
#include "System_Manager.h"

void setup() {
    // Mở Serial ở đây (DUY NHẤT Ở ĐÂY)
    Serial.begin(115200);
    delay(1000); 

    // Gọi hệ thống FreeRTOS
    System_Init();
}

void loop() {
    // Xóa Task loop mặc định để tiết kiệm RAM
    vTaskDelete(NULL);
}