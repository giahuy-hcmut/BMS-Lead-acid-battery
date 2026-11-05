#include "Task_Logic.h"
#include "System_Data.h"
#include <string.h>
#include "Debug_Pins.h"


Logic_Manager::Logic_Manager() {
    isSystemLocked = false; // Mặc định vừa bật máy là chưa khóa
}

void Logic_Manager::init() {
    // Khởi tạo chân Relay
    pinMode(PIN_RELAY_CONTROL, OUTPUT);
    digitalWrite(PIN_RELAY_CONTROL, RELAY_OFF); // Luôn ngắt Relay lúc vừa khởi động để an toàn
    

#ifdef FAULT_INJECT_TEST
    /* INPUT_PULLUP so the rig is a single jumper wire: idle HIGH through the
     * internal pull-up, touch the loose end to GND to inject a fault. GPIO 4
     * has that pull-up; GPIO 34-39 would not. */
    pinMode(PIN_FAULT_INJECT, INPUT_PULLUP);
    Serial.println("[LOGIC] FAULT_INJECT_TEST on GPIO 4 - touch to GND to trip");
#endif

    Serial.println("[LOGIC] Manager Initialized. Relay Configured.");
}

/* Noi them mot ly do vao chuoi, ngan cach " | ". Gom NHIEU loi thay vi chi giu
 * loi dau tien: khi vua mat I2C vua mat CAN thi web phai thay CA HAI. */
static void addReason(char *buf, size_t cap, const char *txt) {
    size_t n = strlen(buf);
    if (n > 0) {
        strncat(buf, " | ", cap - n - 1);
        n = strlen(buf);
    }
    strncat(buf, txt, cap - n - 1);
}

// Hàm cắt điện và in cảnh báo
void Logic_Manager::lockSystem(const char* reason) {
    digitalWrite(PIN_RELAY_CONTROL, RELAY_OFF);

    /* Cap nhat ly do MOI LAN goi, khong chi lan dau: danh sach loi thay doi theo
     * thoi gian (mat them slave, het loi I2C...) va web phai thay hien trang. */
    strncpy(faultReason, reason, sizeof(faultReason) - 1);
    faultReason[sizeof(faultReason) - 1] = '\0';

    if (!isSystemLocked) {          /* chi IN khi CHUYEN trang thai, tranh spam */
        isSystemLocked = true;
        systemLocked   = true;
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

// BỘ NÃO ĐÁNH GIÁ AN TOÀN - gom TAT CA loi dang hoat dong, khong chi loi dau
void Logic_Manager::evaluateProtection() {
#ifdef FAULT_INJECT_TEST
    /* TEST BUILD - the injection pin is the ONLY fault source.
     *
     * Every real check is bypassed on purpose: slave lost, slave hardware
     * error, I2C loss, over-current, low SOC, operator override. The board can
     * then be measured bare on the bench - no slaves on the bus, no INA219, no
     * web interaction - because none of those can hold the relay open.
     *
     * HIGH = healthy -> relay driven ON.   LOW = fault -> relay driven OFF.
     *
     * lockSystem()/unlockSystem() are the production functions, and
     * lockSystem() writes the relay pin on its FIRST statement, so the edge the
     * analyzer sees is the decision itself. Only addReason() is skipped, and
     * that is a sub-microsecond strncat measured against a 10 ms scan period.
     *
     * Scope: detection -> decision, inside the ESP32. Contactor travel time and
     * detection latency (CAN transport, SLAVE_TIMEOUT_MS) are outside the spec
     * and outside this test. */
    if (digitalRead(PIN_FAULT_INJECT) == LOW) {
        lockSystem("TEST Fault Inject");
    } else {
        unlockSystem();
    }
    return;
#else
    BMS_Pack_State snaps[TOTAL_PACKS];
    System_Get_Snapshot(snaps);

    /* Weakest battery limits a series pack. -1 means at least one slave is
     * offline, so the picture is incomplete - see System_MinSoc(). */
    int  sysSoc = System_MinSoc(snaps);
    bool isSafe = true;
    char reasons[sizeof(faultReason)];
    reasons[0] = '\0';

    /* Mat cam bien dong: so dong khong con tin duoc nen KHONG duoc dung no cho
     * phep kiem qua dong ben duoi. INA219 khong co chan ALE (INA226 thi co) nen
     * lop mem nay la bao ve DUY NHAT. */
    bool iValid = !currentSensorFault;

    if (System_Get_RelayOverride()) {
        isSafe = false;
        addReason(reasons, sizeof(reasons), "Operator OFF");
    }
    if (!iValid) {
        isSafe = false;
        addReason(reasons, sizeof(reasons), "Current Sensor Lost (I2C)");
    }

    /* Liet ke DICH DANH binh nao hong thay vi chi bao "co loi": voi 5 binh thi
     * phai biet con nao moi sua duoc. KHONG `break` nua - phai gom het. */
    for (int i = 0; i < TOTAL_PACKS; i++) {
        char tag[24];
        if (!snaps[i].isConnected) {
            isSafe = false;
            snprintf(tag, sizeof(tag), "Lost:0x%03X", CAN_BASE_ID + i);
            addReason(reasons, sizeof(reasons), tag);
        } else if (snaps[i].status != 0x00) {   /* 0x00 = ERROR_NONE */
            isSafe = false;
            snprintf(tag, sizeof(tag), "HwErr:0x%03X", CAN_BASE_ID + i);
            addReason(reasons, sizeof(reasons), tag);
        }
    }

    if (iValid && snaps[0].current > MAX_DISCHARGE_CURRENT) {
        isSafe = false;
        addReason(reasons, sizeof(reasons), "Over Current");
    }

    if (sysSoc >= 0 && sysSoc < MIN_SOC_SHUTDOWN) {
        isSafe = false;
        addReason(reasons, sizeof(reasons), "Battery Depleted (Low SOC)");
    }

    // --- RA QUYẾT ĐỊNH (Cơ chế Hysteresis) ---
    if (!isSafe) {
        lockSystem(reasons);
    } else {
        /* Relay khoi dong o trang thai MO (s_webForceRelayOff = true), nen duong
         * nay luon di qua nhanh hysteresis: sau khi nguoi van hanh bat ON tren
         * web, relay chi dong khi SOC da du an toan.
         * sysSoc == -1 (con binh offline) cung bi chan o day - dung y do. */
        if (sysSoc >= RECOVERY_SOC) {
            unlockSystem();
        }
    }
#endif  /* FAULT_INJECT_TEST */
}

// --- FREE RTOS WRAPPER ---
void Task_Logic_Run(void *pvParameters) {
    Logic_Manager myLogic;
    myLogic.init();

    // Chờ 2 giây để các mạch Slave khởi động và gửi frame CAN đầu tiên,
    // tránh trip oan lúc boot khi chưa bình nào online.
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Task này giờ CHỈ giám sát an toàn. Việc nhập frame -> kho đã tách sang
    // Task_Ingest. Quét theo nhịp CỐ ĐỊNH (không còn ăn ké timeout của queue):
    // nguồn đổi nhanh nhất ở 5ms (dòng) nên 10ms là đủ, vẫn nhường CPU.
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (1) {
        DBG_SET(DBG_LOGIC);
        myLogic.evaluateProtection();
        DBG_CLR(DBG_LOGIC);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PROTECTION_PERIOD_MS));
    }
}