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
    Serial.println("[CURRENT] ACS758 Sensor & Coulomb Counter Initialized");
    lastCalcTime = millis();
}

void CurrentSensor_Manager::loop() {
    // =================================================================
    // [BƯỚC 1]: HIỆU CHUẨN SOC BAN ĐẦU DỰA VÀO ĐIỆN ÁP (OCV METHOD)
    // =================================================================
    Serial.println("[CURRENT] Dang cho dien ap tu mang CAN de khoi tao SOC...");
    float totalStartVolt = 0;
    
    // Vòng lặp chờ đến khi có ít nhất 1 gói tin CAN báo điện áp về
    while(totalStartVolt < VOLTAGE_SYS_MIN_VALID) { 
        BMS_Pack_State snaps[TOTAL_PACKS];
        System_Get_Snapshot(snaps);
        
        totalStartVolt = 0;
        for(int i = 0; i < TOTAL_PACKS; i++) {
            if (snaps[i].isConnected) {
                totalStartVolt += snaps[i].voltage;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Chờ 200ms check lại
    }

    // Nội suy tuyến tính % pin từ điện áp (Tránh Magic Numbers)
    // Nội suy tuyến tính % pin từ điện áp tổng (Tự động scale theo số lượng bình)
    if (totalStartVolt >= VOLTAGE_SYS_100_SOC * TOTAL_PACKS) {
        currentSOC = 100.0;
    } 
    else if (totalStartVolt <= VOLTAGE_SYS_0_SOC * TOTAL_PACKS) {
        currentSOC = 0.0;
    } 
    else {
        currentSOC = ((totalStartVolt - (VOLTAGE_SYS_0_SOC * TOTAL_PACKS)) / 
                     ((VOLTAGE_SYS_100_SOC * TOTAL_PACKS) - (VOLTAGE_SYS_0_SOC * TOTAL_PACKS))) * 100.0;
    }

    // Đồng bộ lại lượng Ah đã dùng tương ứng với % SOC vừa nội suy
    consumedAh = capacityAh * ((100.0 - currentSOC) / 100.0);
    
    Serial.printf("[CURRENT] Dien ap he thong: %.2f V -> SOC ban dau: %.1f %%\n", totalStartVolt, currentSOC);

    // =================================================================
    // [BƯỚC 2]: VÒNG LẶP CHÍNH - ĐẾM COULOMB (COULOMB COUNTING)
    // =================================================================
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

        // 3. THUẬT TOÁN COULOMB COUNTING (Tính % Pin)
        uint32_t currentTime = millis();
        float deltaHours = (currentTime - lastCalcTime) / 3600000.0; 
        lastCalcTime = currentTime;

        consumedAh += (currentA * deltaHours); 
        currentSOC = 100.0 - ((consumedAh / capacityAh) * 100.0);

        if (currentSOC > 100.0) currentSOC = 100.0;
        if (currentSOC < 0.0) currentSOC = 0.0;

        // 4. LƯU VÀO KHO DỮ LIỆU CHUNG
        System_Update_Current(currentA);
        
        if (xSemaphoreTake(dataMutex, 10) == pdTRUE) {
            globalPacks[0].soc = (int)currentSOC;
            xSemaphoreGive(dataMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

// --- FREE RTOS WRAPPER ---
void Task_Current_Run(void *pvParameters) {
    // Đổi tên Macro ở đây cho khớp với Config.h
    CurrentSensor_Manager mySensor(PIN_CURRENT_SENSOR, ACS758_SENSITIVITY, ACS758_ZERO_VOLTAGE, BATTERY_CAPACITY_AH);
    mySensor.init();
    mySensor.loop();
}