#ifndef TASK_ESPNOW_H
#define TASK_ESPNOW_H

#include <Arduino.h>
#include <esp_now.h>
#include "System_Data.h"

// [ĐÃ THAY ĐỔI: Chuyển logic phát sóng thành Class OOP]
class EspNow_Manager {
private:
    uint8_t targetMac[6];
    esp_now_peer_info_t peerInfo;
    BMS_Telemetry_Packet outgoingData;

public:
    EspNow_Manager(const uint8_t* mac);
    bool init();
    void sendTelemetry();
};

void Task_EspNow_Run(void *pvParameters);

#endif