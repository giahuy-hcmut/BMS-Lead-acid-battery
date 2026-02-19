#ifndef TASK_LOGIC_H
#define TASK_LOGIC_H

#include <Arduino.h>
#include "System_Data.h"

// [ĐÃ THAY ĐỔI: Chuyển thành OOP Class để đồng bộ kiến trúc]
class Logic_Manager {
public:
    Logic_Manager();
    void init();
    void processMessage(BMS_Message_t &msg);
};

void Task_Logic_Run(void *pvParameters);

#endif