#include <Arduino.h>
#include "BMS_Receiver.h"

BMS_Data_t myBattery; // Biến lưu dữ liệu pin

void setup() {
    Serial.begin(115200);
    BMS_Init(); // Gọi module CAN
    
    // Sau này thêm: WiFi_Init();
}

void loop() {
    // Gọi hàm đọc liên tục
    if (BMS_Read(&myBattery)) {
        // Chỉ in ra khi có dữ liệu mới
        Serial.print("Voltage: ");
        Serial.print(myBattery.voltage);
        Serial.println(" V");
        
        // Sau này: Blynk_Send(myBattery.voltage);
    }
    
    // Các việc khác...
}