#include "Task_CAN.h"

// --- IMPLEMENTATION CỦA CLASS CAN_MANAGER ---

CAN_Manager::CAN_Manager(int tx, int rx, long baud) {
    txPin = (gpio_num_t)tx;
    rxPin = (gpio_num_t)rx;
    baudRate = baud;
    isReady = false;
}

bool CAN_Manager::init() {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(txPin, rxPin, TWAI_MODE_NORMAL);
    
    // Cấu hình tốc độ
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    if (baudRate == 250000) t_config = TWAI_TIMING_CONFIG_250KBITS();
    else if (baudRate == 1000000) t_config = TWAI_TIMING_CONFIG_1MBITS();
    
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        if (twai_start() == ESP_OK) {
            isReady = true;
            // Chỉ in 1 dòng báo khởi động thành công, không spam
            Serial.println("[CAN] Driver Started Successfully");
            return true;
        }
    }
    
    Serial.println("[CAN] Driver Failed!");
    return false;
}

bool CAN_Manager::readMessage(BMS_Message_t &msgOut) {
    if (!isReady) return false;

    twai_message_t rx_msg;
    // Chờ tối đa 10ms
    if (twai_receive(&rx_msg, pdMS_TO_TICKS(10)) == ESP_OK) {
        
        msgOut.can_id = rx_msg.identifier;
        
        // Giải mã Voltage
        if (rx_msg.data_length_code >= 2) {
            uint16_t raw_vol = (rx_msg.data[0] << 8) | rx_msg.data[1];
            msgOut.voltage = raw_vol / 100.0f;
        } else {
            msgOut.voltage = 0.0f;
        }
        
        return true; 
    }
    return false; 
}


// --- FREE RTOS WRAPPER ---
void Task_CAN_Run(void *pvParameters) {
    // Sử dụng Macro từ System_Data.h (Đã sửa đúng chân)
    CAN_Manager myCanBus(PIN_CAN_TX, PIN_CAN_RX, CAN_BAUD_RATE);

    if (!myCanBus.init()) {
        Serial.println("CAN Init Failed -> Delete Task");
        vTaskDelete(NULL);
    }

    BMS_Message_t tempMsg;

    while (1) {
        if (myCanBus.readMessage(tempMsg)) {
            xQueueSend(canQueue, &tempMsg, 0);
        }
        // Không delay hoặc delay cực ngắn để đảm bảo tốc độ
    }
}