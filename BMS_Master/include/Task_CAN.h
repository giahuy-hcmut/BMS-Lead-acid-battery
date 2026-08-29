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

    // Đọc một frame. CHẶN (portMAX_DELAY) cho tới khi có frame.
    //
    // Trả false KHÔNG còn nghĩa "chưa có frame" - với portMAX_DELAY thì
    // twai_receive không bao giờ timeout. false = LỖI DRIVER (stopped, hoặc
    // đang recover sau bus-off). Người gọi PHẢI delay, nếu không sẽ quay
    // 100% CPU suốt thời gian bus hỏng.
    bool readMessage(BMS_Message_t &msgOut);

    // Gửi frame CAN_MASTER_ID chở dòng pack.
    //   current_a : ampe, >0 = XẢ (quy ước chốt cho cả hệ)
    // Đóng gói: int16 bù 2, MSB trước, A x100 - giống byte 2-3 frame slave.
    void sendCurrentFrame(float current_a);
};

// HAI task, hai bản chất khác nhau - gộp chung chính là lý do bản cũ phải thăm dò:
//   Rx = SỰ KIỆN, chặn trên twai_receive, ngủ 0% CPU khi bus im
//   Tx = CHU KỲ,  vTaskDelayUntil, nhịp 0x100 không trôi
//
// Gộp một task thì hai việc đánh nhau: chặn để nhận ⇒ không bao giờ tới lượt gửi;
// muốn gửi đúng nhịp ⇒ buộc phải thăm dò 1000 lần/giây, ~995 lần hỏi không.
void Task_CAN_Rx_Run(void *pvParameters);
void Task_CAN_Tx_Run(void *pvParameters);

#endif