#ifndef TASK_TERMINAL_H
#define TASK_TERMINAL_H

#include <Arduino.h>
#include "System_Data.h"

class Terminal_Remote_Dashboard {
private:
    Remote_System_State localState;
    void printDashboard();

public:
    Terminal_Remote_Dashboard();
    void init();
    void loop();
};

void Task_Terminal_Run(void *pvParameters);

#endif