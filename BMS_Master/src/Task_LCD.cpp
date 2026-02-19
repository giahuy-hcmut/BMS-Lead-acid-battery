#include "Task_LCD.h"
#include <Wire.h>
#include "System_Data.h"

LCD_Manager::LCD_Manager(uint8_t addr, uint8_t cols, uint8_t rows) {
    lcd = new LiquidCrystal_I2C(addr, cols, rows);
    currentPage = 0;
    refreshCounter = 0;
    // Khởi tạo bộ đệm
    for(int i=0; i<TOTAL_PACKS; i++) {
        localPacks[i].lastUpdate = 0;
        localPacks[i].voltage = 0;
        localPacks[i].isConnected = false;
    }
}

void LCD_Manager::init() {
    // [CHUẨN CÔNG NGHIỆP] Khởi động I2C với chân từ Config
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(10000); 
    
    lcd->init(); 
    lcd->backlight();
    lcd->setCursor(0, 0);
    lcd->print("BMS MASTER V3.0");
    lcd->setCursor(0, 1);
    lcd->print("Industrial Std.");
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void LCD_Manager::fetchData() {
    // [CHUẨN CÔNG NGHIỆP] Lấy bản chụp an toàn
    System_Get_Snapshot(localPacks);

    totalVolt = 0;
    activeCount = 0;

    for(int i=0; i<TOTAL_PACKS; i++) {
        bool isOnline = (localPacks[i].lastUpdate > 0) && 
                        (millis() - localPacks[i].lastUpdate < LCD_TIMEOUT);
        localPacks[i].isConnected = isOnline;
        
        if (isOnline) {
            totalVolt += localPacks[i].voltage;
            activeCount++;
        }
    }
}

// (Các hàm in màn hình giữ nguyên như cũ vì chỉ là logic hiển thị)
void LCD_Manager::checkHealth() {
    refreshCounter++;
    // [ĐÃ THAY ĐỔI: Thay số 10 gõ cứng bằng Macro LCD_I2C_RECOVERY_TICKS từ Config.h]
    if (refreshCounter > LCD_I2C_RECOVERY_TICKS) {
        Wire.beginTransmission(LCD_ADDR);
        if (Wire.endTransmission() == 0) {
            lcd->init(); lcd->backlight(); 
        }
        refreshCounter = 0;
    }
}

void LCD_Manager::drawSummary() {
    lcd->setCursor(0, 0);
    lcd->print("TOTAL: "); lcd->print(totalVolt, 1); lcd->print("V");
    lcd->setCursor(0, 1);
    lcd->print("Active: "); lcd->print(activeCount); lcd->print("/"); lcd->print(TOTAL_PACKS); lcd->print(" Pks");
}

void LCD_Manager::drawDetail(int packIndex) {
    int currentID = CAN_BASE_ID + packIndex;
    lcd->setCursor(0, 0);
    lcd->printf("PACK %d (0x%X)", packIndex + 1, currentID);
    lcd->setCursor(0, 1);
    if (localPacks[packIndex].isConnected) {
        lcd->print("Vol: "); lcd->print(localPacks[packIndex].voltage, 2); lcd->print("V");
    } else {
        lcd->print("DISCONNECTED !");
    }
}

void LCD_Manager::loop() {
    while (1) {
        checkHealth();
        fetchData();
        
        lcd->clear();
        if (currentPage == 0) {
            drawSummary();
        } else {
            drawDetail(currentPage - 1);
        }

        currentPage++;
        if (currentPage > TOTAL_PACKS) currentPage = 0;

        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

// --- FREE RTOS WRAPPER ---
// [ĐÃ THAY ĐỔI: Thêm hàm Wrapper thực tế để main.cpp có thể gọi được]
void Task_LCD_Run(void *pvParameters) {
    LCD_Manager myLCD(LCD_ADDR, LCD_COLS, LCD_ROWS);
    myLCD.init();
    myLCD.loop();
}