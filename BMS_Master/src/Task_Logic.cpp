#include "Task_Logic.h"
#include "System_Data.h"

void Task_Logic_Run(void *pvParameters) {
    BMS_Message_t msg;

    while (1) {
        // Chờ tin nhắn từ hàng đợi (Block vô hạn nếu không có tin)
        if (xQueueReceive(canQueue, &msg, portMAX_DELAY) == pdTRUE) {
            
            // [CHUẨN CÔNG NGHIỆP]
            // Gọi API cập nhật an toàn, không cần lo tính toán index hay mutex
            System_Update_Pack(msg.can_id, msg.voltage);
        }
    }
}