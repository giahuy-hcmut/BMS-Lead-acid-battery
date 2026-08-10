#ifndef TASK_CURRENT_H
#define TASK_CURRENT_H

#include <Arduino.h>
#include "Config.h"
#include "System_Data.h"

// SOC gio do TUNG SLAVE tinh bang Kalman roi gui ve trong byte 4 cua frame CAN.
// Class nay chi con MOT viec: do dong dien pack. Bo dem Coulomb cu da bi go -
// hai nguon SOC ghi vao cung mot cho se danh nhau.
class CurrentSensor_Manager {
private:
    int pin;
    float sensitivity;
    float zeroVoltage;

    // Bộ lọc trung bình động (Moving Average Filter) chống nhiễu ADC
    static const int NUM_SAMPLES = 50;
    float samples[NUM_SAMPLES];
    int sampleIndex;
    float totalSum;

public:
    CurrentSensor_Manager(int adcPin, float sens, float zeroV);
    void init();
    void loop();
};

void Task_Current_Run(void *pvParameters);

#endif