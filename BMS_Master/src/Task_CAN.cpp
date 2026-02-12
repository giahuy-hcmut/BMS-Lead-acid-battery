#include "Task_CAN.h"
#include "System_Data.h"
#include "driver/twai.h"

// Cấu hình chân CAN (ESP32 thường dùng 16, 17 hoặc 21, 22 - Kiểm tra lại mạch của bạn)
#define CAN_TX_PIN 16
#define CAN_RX_PIN 17

void Task_CAN_Run(void *pvParameters) {
    // 1. Khởi tạo Driver CAN (TWAI)
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        twai_start();
        Serial.println("[CAN] Driver Started");
    } else {
        Serial.println("[CAN] Driver Failed!");
        vTaskDelete(NULL); // Hủy Task nếu lỗi
    }

    BMS_Message_t msg;
    twai_message_t rx_msg;

    while (1) {
        // 2. Chờ nhận tin (Block tối đa 10ms)
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(10)) == ESP_OK) {
            
            // 3. Giải mã thô
            msg.can_id = rx_msg.identifier;
            
            // Ví dụ: Byte 0-1 là Voltage (nhân 100)
            if (rx_msg.data_length_code >= 2) {
                uint16_t raw_vol = (rx_msg.data[0] << 8) | rx_msg.data[1];
                msg.voltage = raw_vol / 100.0f;
            }
            
            // 4. Đẩy vào Queue (Gửi ngay lập tức)
            xQueueSend(canQueue, &msg, 0);
        }
    }
}