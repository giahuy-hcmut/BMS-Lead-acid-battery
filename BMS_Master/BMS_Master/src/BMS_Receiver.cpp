#include "BMS_Receiver.h"

void BMS_Init() {
    // 1. Cấu hình
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // 2. Cài đặt Driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        Serial.println("[CAN] Driver installed");
    } else {
        Serial.println("[CAN] Install Failed!");
        return;
    }

    // 3. Start
    if (twai_start() == ESP_OK) {
        Serial.println("[CAN] Driver started");
    } else {
        Serial.println("[CAN] Start Failed!");
    }
}

bool BMS_Read(BMS_Data_t *dataOut) {
    twai_message_t message;
    
    // Kiểm tra xem có tin nhắn không (Non-blocking hoặc timeout cực ngắn)
    if (twai_receive(&message, 0) == ESP_OK) { // 0 ticks = không chờ
        
        if (message.identifier == BMS_SLAVE_ID) {
            // Giải mã: Ghép 2 byte -> chia 100
            uint16_t raw = (message.data[0] << 8) | message.data[1];
            dataOut->voltage = raw / 100.0f;
            
            // Cập nhật trạng thái
            dataOut->isConnected = true;
            dataOut->lastUpdate = millis();
            return true; // Có dữ liệu mới
        }
    }
    return false; // Không có gì mới
}