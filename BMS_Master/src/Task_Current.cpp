#include "Task_Current.h"

CurrentSensor_Manager::CurrentSensor_Manager(int adcPin, float sens, float zeroV, float capAh) {
    pin = adcPin;
    sensitivity = sens;
    zeroVoltage = zeroV;
    capacityAh = capAh;
    
    consumedAh = 0.0;
    currentSOC = 100.0; // Giả sử khi vừa bật máy là pin đầy 100%
    lastCalcTime = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) samples[i] = 0;
    sampleIndex = 0;
    totalSum = 0;
}

void CurrentSensor_Manager::init() {
    analogReadResolution(12); // ESP32 ADC 12-bit (0-4095)
    pinMode(pin, INPUT);
    Serial.println("[CURRENT] ACS712 Sensor & Coulomb Counter Initialized");
    lastCalcTime = millis();
}

void CurrentSensor_Manager::loop() {
    while (1) {
        // 1. ĐỌC ADC VÀ LỌC NHIỄU (Moving Average)
        totalSum = totalSum - samples[sampleIndex];
        
        // Chuyển giá trị ADC (0-4095) sang Volt (Hệ quy chiếu 3.3V)
        float adcVolt = (analogRead(pin) / 4095.0) * 3.3;
        
        samples[sampleIndex] = adcVolt;
        totalSum = totalSum + samples[sampleIndex];
        sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;

        float avgVolt = totalSum / NUM_SAMPLES;

        // 2. TÍNH DÒNG ĐIỆN
        float currentA = (avgVolt - zeroVoltage) / sensitivity;
        
        // [QUAN TRỌNG] Khử nhiễu vùng không (Deadband)
        // Nếu dòng điện đọc được < 0.2A, ta coi như motor đang tắt để tránh SOC bị trừ dần do nhiễu
        if (abs(currentA) < 0.3) {
            currentA = 0.0;
        }

        // 3. THUẬT TOÁN COULOMB COUNTING (Tính % Pin)
        uint32_t currentTime = millis();
        // Đổi thời gian từ milli-giây sang Giờ (Hours)
        float deltaHours = (currentTime - lastCalcTime) / 3600000.0; 
        lastCalcTime = currentTime;

        // Tích phân: Điện lượng = Dòng điện * Thời gian
        consumedAh += (currentA * deltaHours); 
        currentSOC = 100.0 - ((consumedAh / capacityAh) * 100.0);

        // Khóa giới hạn an toàn
        if (currentSOC > 100.0) currentSOC = 100.0;
        if (currentSOC < 0.0) currentSOC = 0.0;

        // 4. LƯU VÀO KHO DỮ LIỆU CHUNG
        System_Update_Current(currentA);
        
        // Mượn pack 0 để lưu trữ SOC tổng (không làm xáo trộn struct cũ)
        if (xSemaphoreTake(dataMutex, 10) == pdTRUE) {
            globalPacks[0].soc = (int)currentSOC;
            xSemaphoreGive(dataMutex);
        }

        // Lấy mẫu siêu tốc: 10ms/lần (100Hz) để không bỏ sót bất kỳ biến động nào của motor
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

// --- FREE RTOS WRAPPER ---
void Task_Current_Run(void *pvParameters) {
    CurrentSensor_Manager mySensor(PIN_CURRENT_SENSOR, ACS712_SENSITIVITY, ACS712_ZERO_VOLTAGE, BATTERY_CAPACITY_AH);
    mySensor.init();
    mySensor.loop();
}