#include "System_Data.h"

Remote_System_State globalSystemState;
SemaphoreHandle_t dataMutex;
QueueHandle_t espNowQueue;

void System_Data_Init() {
    dataMutex = xSemaphoreCreateMutex();
    // Tạo hàng đợi chứa được 5 gói tin dự phòng
    espNowQueue = xQueueCreate(5, sizeof(BMS_Telemetry_Packet));

    globalSystemState.lastRecvTime = 0;
    globalSystemState.isConnected = false;
    memset(&globalSystemState.telemetry, 0, sizeof(BMS_Telemetry_Packet));
}

void System_Update_Telemetry(const BMS_Telemetry_Packet* incomingData) {
    if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
        memcpy(&globalSystemState.telemetry, incomingData, sizeof(BMS_Telemetry_Packet));
        globalSystemState.lastRecvTime = millis();
        globalSystemState.isConnected = true;
        xSemaphoreGive(dataMutex);
    }
}

void System_Get_Snapshot(Remote_System_State* snapshot) {
    if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
        memcpy(snapshot, &globalSystemState, sizeof(Remote_System_State));
        
        // Cập nhật trạng thái mất sóng
        if (millis() - snapshot->lastRecvTime > CONNECTION_TIMEOUT && snapshot->lastRecvTime != 0) {
            snapshot->isConnected = false;
        }
        xSemaphoreGive(dataMutex);
    }
}