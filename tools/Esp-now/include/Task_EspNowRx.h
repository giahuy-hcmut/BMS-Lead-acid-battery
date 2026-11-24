#ifndef TASK_ESPNOW_RX_H
#define TASK_ESPNOW_RX_H

#include <Arduino.h>
#include <esp_now.h>
#include "System_Data.h"

class EspNowRx_Manager {
public:
    EspNowRx_Manager();
    bool init();
    void processQueue();
};

void Task_EspNowRx_Run(void *pvParameters);

#endif