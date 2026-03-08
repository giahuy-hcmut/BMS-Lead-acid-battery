#include "Task_Logic.h"
#include "System_Data.h"

Logic_Manager::Logic_Manager() {
    isSystemLocked = false; // Mặc định vừa bật máy là chưa khóa
}

void Logic_Manager::init() {
    // Khởi tạo chân Relay
    pinMode(PIN_RELAY_CONTROL, OUTPUT);
    digitalWrite(PIN_RELAY_CONTROL, RELAY_OFF); // Luôn ngắt Relay lúc vừa khởi động để an toàn
    
    Serial.println("[LOGIC] Manager Initialized. Relay Configured.");
}

void Logic_Manager::processMessage(BMS_Message_t &msg) {
    System_Update_Pack(msg.can_id, msg.voltage, msg.temperature, msg.status);
}

// Hàm cắt điện và in cảnh báo
void Logic_Manager::lockSystem(const char* reason) {
    digitalWrite(PIN_RELAY_CONTROL, RELAY_OFF); // CẮT ĐIỆN!
    if (!isSystemLocked) {
        isSystemLocked = true;
        Serial.printf("\n[PROTECTION] RELAY TRIPPED! Reason: %s\n", reason);
    }
}

// Hàm cấp điện trở lại
void Logic_Manager::unlockSystem() {
    digitalWrite(PIN_RELAY_CONTROL, RELAY_ON); // ĐÓNG ĐIỆN!
    if (isSystemLocked) {
        isSystemLocked = false;
        Serial.println("\n[PROTECTION] SYSTEM RECOVERED. RELAY ON.");
    }
}

// BỘ NÃO ĐÁNH GIÁ AN TOÀN - Chạy mỗi 100ms
void Logic_Manager::evaluateProtection() {
    BMS_Pack_State snaps[TOTAL_PACKS];
    System_Get_Snapshot(snaps);

    bool isSafe = true;
    const char* errorReason = "";

    // 1. Kiểm tra Mất kết nối & Lỗi phần cứng (Từ Slave gửi lên)
    for (int i = 0; i < TOTAL_PACKS; i++) {
        if (!snaps[i].isConnected) {
            isSafe = false;
            errorReason = "CAN Timeout / Slave Lost";
            break;
        }
        if (snaps[i].status != 0x00) { // 0x00 là ERROR_NONE
            isSafe = false;
            errorReason = "Hardware Error from Slave (OVP/UVP/OTP)";
            break;
        }
    }

    // 2. Kiểm tra Quá dòng (Over-current)
    if (isSafe && snaps[0].current > MAX_DISCHARGE_CURRENT) {
        isSafe = false;
        errorReason = "Over Current Detected";
    }

    // 3. Kiểm tra cạn kiệt năng lượng (Low SOC)
    if (isSafe && snaps[0].soc < MIN_SOC_SHUTDOWN) {
        isSafe = false;
        errorReason = "Battery Depleted (Low SOC)";
    }

    // --- RA QUYẾT ĐỊNH (Cơ chế Hysteresis) ---
    if (!isSafe) {
        lockSystem(errorReason); // Ngắt lập tức nếu có bất kỳ lỗi gì
    } 
    else {
        // Nếu hệ thống đang bình thường TRỞ LẠI
        // (Chỉ cho phép mở lại Relay nếu SOC đã sạc lên mức an toàn, tránh bật tắt liên tục)
        if (isSystemLocked) {
            if (snaps[0].soc >= RECOVERY_SOC) {
                unlockSystem(); 
            }
        } else {
            // Khởi động trơn tru: Lần đầu tiên bật máy, nếu mọi thứ OK thì đóng Relay
            unlockSystem();
        }
    }
}

// --- FREE RTOS WRAPPER ---
void Task_Logic_Run(void *pvParameters) {
    Logic_Manager myLogic;
    myLogic.init();
    
    BMS_Message_t msg;

    // Chờ 2 giây để các mạch Slave khởi động và gửi dữ liệu CAN đầu tiên
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        // [ĐÃ THAY ĐỔI QUAN TRỌNG] 
        // Đợi tin nhắn CAN tối đa 100ms. 
        // Nếu có tin nhắn: Xử lý ngay lập tức (Real-time).
        // Nếu không có: Thoát ra chạy tiếp để đi đánh giá an toàn (Timeout protection).
        if (xQueueReceive(canQueue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            myLogic.processMessage(msg);
        }
        
        // Quét bảo vệ liên tục
        myLogic.evaluateProtection();
    }
}