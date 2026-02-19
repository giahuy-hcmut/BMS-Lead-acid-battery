#ifndef TASK_LCD_H
#define TASK_LCD_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "System_Data.h"

class LCD_Remote_Manager {
private:
    LiquidCrystal_I2C* lcd;
    Remote_System_State localState;

    void drawDashboard();
    void drawLostConnection();

public:
    LCD_Remote_Manager(uint8_t addr, uint8_t cols, uint8_t rows);
    void init();
    void loop();
};

void Task_LCD_Run(void *pvParameters);

#endif