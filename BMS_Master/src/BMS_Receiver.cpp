#include "BMS_Receiver.h"

void BMS_Driver_Init() {
    // 1. Cấu hình
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    
    // ACCEPT_ALL: Nhận tất cả ID để xử lý nhiều Slave
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // 2. Cài đặt Driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        Serial.println("[DRIVER] CAN Installed");
    } else {
        Serial.println("[DRIVER] CAN Install Failed!");
        return;
    }

    // 3. Start
    if (twai_start() == ESP_OK) {
        Serial.println("[DRIVER] CAN Started");
    } else {
        Serial.println("[DRIVER] CAN Start Failed!");
    }
}

bool BMS_Driver_Read(BMS_Message_t *msgOut) {
    twai_message_t message;
    
    // Non-blocking read (Timeout = 0)
    if (twai_receive(&message, 0) == ESP_OK) {
        
        // Đóng gói dữ liệu
        msgOut->can_id = message.identifier;
        
        // Giải mã: (HighByte << 8) | LowByte -> chia 100
        uint16_t raw = (message.data[0] << 8) | message.data[1];
        msgOut->voltage = raw / 100.0f;
        
        msgOut->timestamp = millis();
        return true;
    }
    return false;
}