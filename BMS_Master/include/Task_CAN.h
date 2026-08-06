#ifndef TASK_CAN_H
#define TASK_CAN_H

#include <Arduino.h>
#include "driver/twai.h"
#include "System_Data.h" // Để lấy kiểu BMS_Message_t

// Class quản lý phần cứng CAN (Driver Layer)
class CAN_Manager {
private:
    gpio_num_t txPin;
    gpio_num_t rxPin;
    long baudRate;
    bool isReady;

public:
    // Constructor: Cho phép tùy chỉnh chân và tốc độ ngay khi tạo đối tượng
    CAN_Manager(int tx, int rx, long baud = 500000);

    // Khởi động Driver
    bool init();

    // Hàm đọc tin nhắn (Non-blocking)
    // Trả về true nếu có tin, false nếu không
    bool readMessage(BMS_Message_t &msgOut);

    // Gửi frame CAN_MASTER_ID chở dòng pack.
    //   current_a : ampe, >0 = XẢ (quy ước chốt cho cả hệ)
    // Đóng gói: int16 bù 2, MSB trước, A x100 - giống byte 2-3 frame slave.
    void sendCurrentFrame(float current_a);
};

// Hàm Wrapper cho FreeRTOS gọi
void Task_CAN_Run(void *pvParameters);

#endif