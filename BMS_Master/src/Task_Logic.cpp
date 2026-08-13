#include "Task_Logic.h"
#include "System_Data.h"
#include <string.h>

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
    System_Update_Pack(msg.can_id, msg.voltage, msg.temperature,
                       msg.status, msg.soc);
}

// Hàm cắt điện và in cảnh báo
void Logic_Manager::lockSystem(const char* reason) {
    digitalWrite(PIN_RELAY_CONTROL, RELAY_OFF);
    if (!isSystemLocked) {
        isSystemLocked = true;
        systemLocked   = true;
        strncpy(faultReason, reason, sizeof(faultReason) - 1);
        Serial.printf("\n[PROTECTION] RELAY TRIPPED! Reason: %s\n", reason);
    }
}

// Hàm cấp điện trở lại
void Logic_Manager::unlockSystem() {
    digitalWrite(PIN_RELAY_CONTROL, RELAY_ON);
    if (isSystemLocked) {
        isSystemLocked = false;
        systemLocked   = false;
        faultReason[0] = '\0';
        Serial.println("\n[PROTECTION] SYSTEM RECOVERED. RELAY ON.");
    }
}

// BỘ NÃO ĐÁNH GIÁ AN TOÀN - Chạy mỗi 100ms
void Logic_Manager::evaluateProtection() {
    if (System_Get_RelayOverride()) {
        lockSystem("Web Manual Override");
        return;
    }

    /* Mat cam bien dong -> ngat ngay, khong xet gi them. So dong chinh la dau vao
     * cua phep kiem qua dong ben duoi, nen mat no la mat bao ve. INA219 khong co
     * chan ALE (INA226 thi co) nen day la lop bao ve DUY NHAT.
     *
     * Return som theo dung khuon webForceRelayOff. Khi co tat, luong chay tiep
     * xuong phan hysteresis binh thuong nen relay van phai cho SOC >= RECOVERY_SOC
     * moi dong lai. */
    if (currentSensorFault) {
        lockSystem("Current Sensor Lost (I2C)");
        return;
    }

    BMS_Pack_State snaps[TOTAL_PACKS];
    System_Get_Snapshot(snaps);

    /* Weakest battery limits a series pack. -1 means at least one slave is
     * offline, so the picture is incomplete - see System_MinSoc(). */
    int sysSoc = System_MinSoc(snaps);

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
    if (isSafe && sysSoc >= 0 && sysSoc < MIN_SOC_SHUTDOWN) {
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
            /* sysSoc == -1 keeps the relay open on purpose: do not re-energise
             * while any battery is unmonitored. */
            if (sysSoc >= RECOVERY_SOC) {
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