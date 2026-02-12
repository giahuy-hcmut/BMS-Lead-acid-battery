#include "Task_CAN.h"

// --- IMPLEMENTATION CỦA CLASS CAN_MANAGER ---

// 1. Constructor: Chỉ lưu cấu hình, chưa chọc vào phần cứng
CAN_Manager::CAN_Manager(int tx, int rx, long baud) {
    txPin = (gpio_num_t)tx;
    rxPin = (gpio_num_t)rx;
    baudRate = baud;
    isReady = false;
}

// 2. Init: Cấu hình Driver TWAI (Phần này thường rất dài dòng, giờ đã được giấu đi)
bool CAN_Manager::init() {
    // Config chung
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(txPin, rxPin, TWAI_MODE_NORMAL);
    
    // Config tốc độ (Mặc định 500Kbps)
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    if (baudRate == 250000) t_config = TWAI_TIMING_CONFIG_250KBITS();
    else if (baudRate == 1000000) t_config = TWAI_TIMING_CONFIG_1MBITS();
    
    // Config bộ lọc (Nhận tất cả)
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Cài đặt driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        if (twai_start() == ESP_OK) {
            isReady = true;
            Serial.println("[CAN-OOP] Driver Started Successfully");
            return true;
        }
    }
    
    Serial.println("[CAN-OOP] Driver Failed!");
    return false;
}

// 3. readMessage: Đọc và chuyển đổi dữ liệu
bool CAN_Manager::readMessage(BMS_Message_t &msgOut) {
    if (!isReady) return false;

    twai_message_t rx_msg;
    // Chờ tối đa 10ms (Non-blocking nhưng vẫn có timeout nhẹ để đỡ tốn CPU)
    if (twai_receive(&rx_msg, pdMS_TO_TICKS(10)) == ESP_OK) {
        
        // Giải mã dữ liệu thô sang Struct chuẩn
        msgOut.can_id = rx_msg.identifier;
        
        // Ví dụ: Byte 0-1 là Voltage (High byte first)
        if (rx_msg.data_length_code >= 2) {
            uint16_t raw_vol = (rx_msg.data[0] << 8) | rx_msg.data[1];
            msgOut.voltage = raw_vol / 100.0f;
        } else {
            msgOut.voltage = 0.0f;
        }
        
        return true; // Báo là có tin mới
    }
    return false; // Không có tin
}


// --- FREE RTOS WRAPPER (Cầu nối giữa OOP và OS) ---
// Đây là nơi "ghép" Class CAN với Queue của hệ điều hành
void Task_CAN_Run(void *pvParameters) {
    // 1. Tạo đối tượng (Instantiate)
    // Truyền chân 16, 17 vào đây -> Rất linh hoạt!
    CAN_Manager myCanBus(16, 17, 500000);

    // 2. Khởi động
    if (!myCanBus.init()) {
        Serial.println("CAN Init Failed -> Delete Task");
        vTaskDelete(NULL); // Tự sát nếu không khởi động được
    }

    BMS_Message_t tempMsg;

    // 3. Vòng lặp vô tận (Loop)
    while (1) {
        // Gọi hàm đọc của Class
        if (myCanBus.readMessage(tempMsg)) {
            // Nếu có tin -> Đẩy vào Queue
            // (Class CAN_Manager không biết Queue là gì, Task này lo việc đó)
            xQueueSend(canQueue, &tempMsg, 0);
        }

        // Delay cực ngắn để nhường CPU nếu không có tin
        // (Hàm readMessage đã có delay 10ms bên trong rồi nên ở đây delay ít thôi hoặc ko cần)
        // vTaskDelay(1); 
    }
}