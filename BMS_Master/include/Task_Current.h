#ifndef TASK_CURRENT_H
#define TASK_CURRENT_H

#include <Arduino.h>
#include "Config.h"
#include "System_Data.h"

class CurrentSensor_Manager {
private:
    int pin;
    float sensitivity;
    float zeroVoltage;
    float capacityAh;
    
    // Biến cho Coulomb Counting
    float consumedAh; 
    float currentSOC;
    uint32_t lastCalcTime;

    // Bộ lọc trung bình động (Moving Average Filter) chống nhiễu ADC
    static const int NUM_SAMPLES = 50;
    float samples[NUM_SAMPLES];
    int sampleIndex;
    float totalSum;

public:
    CurrentSensor_Manager(int adcPin, float sens, float zeroV, float capAh);
    void init();
    void loop();
};

void Task_Current_Run(void *pvParameters);

#endif