#include "Task_Logic.h"
#include "System_Data.h"

// [ĐÃ THAY ĐỔI: Thêm các hàm của Class Logic_Manager theo chuẩn OOP]
Logic_Manager::Logic_Manager() {
    // Constructor trống, chuẩn bị cho sau này tính SOC
}

void Logic_Manager::init() {
    Serial.println("[LOGIC] Manager Initialized");
}

void Logic_Manager::processMessage(BMS_Message_t &msg) {
    // [CHUẨN CÔNG NGHIỆP]
    // Gọi API cập nhật an toàn, không cần lo tính toán index hay mutex
    System_Update_Pack(msg.can_id, msg.voltage);
}

// [ĐÃ THAY ĐỔI: Viết lại Wrapper FreeRTOS để sử dụng Object OOP]
void Task_Logic_Run(void *pvParameters) {
    Logic_Manager myLogic;
    myLogic.init();
    
    BMS_Message_t msg;

    while (1) {
        // Chờ tin nhắn từ hàng đợi (Block vô hạn nếu không có tin)
        if (xQueueReceive(canQueue, &msg, portMAX_DELAY) == pdTRUE) {
            // Đẩy tin nhắn vào class để xử lý
            myLogic.processMessage(msg);
        }
    }
}