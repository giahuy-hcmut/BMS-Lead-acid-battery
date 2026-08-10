#include "Task_Current.h"

CurrentSensor_Manager::CurrentSensor_Manager(int adcPin, float sens, float zeroV) {
    pin = adcPin;
    sensitivity = sens;
    zeroVoltage = zeroV;

    for (int i = 0; i < NUM_SAMPLES; i++) samples[i] = 0;
    sampleIndex = 0;
    totalSum = 0;
}

void CurrentSensor_Manager::init() {
    analogReadResolution(12); // ESP32 ADC 12-bit (0-4095)
    pinMode(pin, INPUT);
    Serial.println("[CURRENT] Pack current sensor initialized");
}

void CurrentSensor_Manager::loop() {
    /* One job: measure the pack current and publish it.
     *
     * What used to be here and is now gone:
     *   - a blocking wait for the first CAN voltage, only there to seed SOC
     *   - an OCV interpolation for the initial SOC
     *   - Coulomb integration
     *   - an unsynchronised globalPacks[0].soc write, outside the mutex that
     *     System_Get_Snapshot() takes to read the very same field
     *
     * Each slave now runs its own Kalman filter and reports SOC in byte 4.
     * Dropping the wait also means the current is measured from boot instead
     * of after the first CAN frame - the slaves need it early. */
    while (1) {
        // 1. ĐỌC ADC VÀ LỌC NHIỄU (Moving Average)
        totalSum = totalSum - samples[sampleIndex];
        float adcVolt = (analogRead(pin) / 4095.0) * 3.3;
        
        samples[sampleIndex] = adcVolt;
        totalSum = totalSum + samples[sampleIndex];
        sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;

        float avgVolt = totalSum / NUM_SAMPLES;

        // 2. TÍNH DÒNG ĐIỆN
        float currentA = (avgVolt - zeroVoltage) / sensitivity;
        
        // Khử nhiễu vùng không bằng Macro bạn đã định nghĩa
        if (abs(currentA) < ACS758_ZERO_CURRENT) {
            currentA = 0.0;
        }

        // 3. LƯU VÀO KHO DỮ LIỆU CHUNG
        System_Update_Current(currentA);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- FREE RTOS WRAPPER ---
void Task_Current_Run(void *pvParameters) {
    // Đổi tên Macro ở đây cho khớp với Config.h
    CurrentSensor_Manager mySensor(PIN_CURRENT_SENSOR, ACS758_SENSITIVITY, ACS758_ZERO_VOLTAGE);
    mySensor.init();
    mySensor.loop();
}