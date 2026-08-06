#include "System_Data.h"

// 1. Biến toàn cục thực tế
BMS_Pack_State globalPacks[TOTAL_PACKS];
SemaphoreHandle_t dataMutex;
QueueHandle_t canQueue;
volatile bool webForceRelayOff = false;
volatile bool webSlavesActive  = true;
volatile bool systemLocked     = false;
char faultReason[64]           = "";

// 2. Khởi tạo
void System_Data_Init() {
    dataMutex = xSemaphoreCreateMutex();
    // [ĐÃ THAY ĐỔI: Thay số 20 cứng bằng Macro CAN_QUEUE_LENGTH]
    canQueue = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(BMS_Message_t));
    
    // Xóa sạch dữ liệu ban đầu
    for(int i=0; i<TOTAL_PACKS; i++) {
        globalPacks[i].voltage = 0;
        globalPacks[i].current = 0;
        globalPacks[i].soc = 0;
        globalPacks[i].lastUpdate = 0;
        globalPacks[i].isConnected = false;
    }
}

// --- HÀM GHI AN TOÀN (Dành cho Task Logic) ---
void System_Update_Pack(uint32_t can_id, float voltage, int8_t temp, uint8_t status) {
    int idx = can_id - CAN_BASE_ID;
    
    if (idx < 0 || idx >= TOTAL_PACKS) return;

    if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
        globalPacks[idx].voltage = voltage;
        globalPacks[idx].temperature = temp; // Lưu nhiệt độ
        globalPacks[idx].status = status;    // Lưu mã lỗi
        globalPacks[idx].lastUpdate = millis();
        xSemaphoreGive(dataMutex);
    }
}

void System_Update_Current(float current) {
    if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
        for(int i=0; i<TOTAL_PACKS; i++) {
            globalPacks[i].current = current;
        }
        xSemaphoreGive(dataMutex);
    }
}

float System_Get_Current(void) {
    static float s_cached = 0.0f;   /* only Task_CAN calls this */

    /* Timeout 0: this sits on the 5 ms frame path and must never block.
     * On contention keep the last known value rather than returning 0.0, so a
     * busy mutex cannot inject a spurious "0 A" into every slave's filter.
     * globalPacks[0] because System_Update_Current() writes the same value to
     * all packs - the same current flows through batteries in series. */
    if (xSemaphoreTake(dataMutex, 0) == pdTRUE) {
        s_cached = globalPacks[0].current;
        xSemaphoreGive(dataMutex);
    }
    return s_cached;
}

// --- HÀM ĐỌC AN TOÀN (Dành cho Task Hiển thị) ---
// --- HÀM ĐỌC AN TOÀN (Dành cho Task Hiển thị) ---
void System_Get_Snapshot(BMS_Pack_State *snapshotArray) {
    if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
        for(int i = 0; i < TOTAL_PACKS; i++) {
            // Tự động tính trạng thái Online ngay tại lõi
            bool isOnline = (globalPacks[i].lastUpdate > 0) && 
                            (millis() - globalPacks[i].lastUpdate < LCD_TIMEOUT);
            globalPacks[i].isConnected = isOnline;
            
            // Copy ra bản nháp cho các Task khác dùng
            snapshotArray[i] = globalPacks[i];
        }
        xSemaphoreGive(dataMutex);
    }
}