#ifndef TASK_LOGIC_H
#define TASK_LOGIC_H

#include <Arduino.h>
#include "System_Data.h"

class Logic_Manager {
private:
    bool isSystemLocked;    // Cờ trạng thái: true = đang bị khóa do lỗi
    void lockSystem(const char* reason);   // Hàm thực thi ngắt Relay
    void unlockSystem();                   // Hàm thực thi đóng Relay

public:
    Logic_Manager();
    void init();
    void processMessage(BMS_Message_t &msg);
    void evaluateProtection(); // Hàm liên tục quét lỗi hệ thống
};

void Task_Logic_Run(void *pvParameters);

#endif