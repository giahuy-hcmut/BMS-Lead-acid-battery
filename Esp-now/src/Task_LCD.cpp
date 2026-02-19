#include "Task_LCD.h"
#include <Wire.h>

LCD_Remote_Manager::LCD_Remote_Manager(uint8_t addr, uint8_t cols, uint8_t rows) {
    lcd = new LiquidCrystal_I2C(addr, cols, rows);
    refreshCounter = 0;
}

void LCD_Remote_Manager::init() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    lcd->init();
    lcd->backlight();
    lcd->setCursor(0, 0);
    lcd->print("MURATA EV REMOTE");
    lcd->setCursor(0, 1);
    lcd->print("Waiting Signal..");
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void LCD_Remote_Manager::drawDashboard() {
    // Dòng 1: In Tổng Áp và Dòng Điện
    lcd->setCursor(0, 0);
    lcd->print("V:");
    lcd->print(localState.telemetry.totalVoltage, 1);
    lcd->print(" I:");
    lcd->print(localState.telemetry.systemCurrent, 1);
    lcd->print("A   "); // Dấu cách thừa để xóa chữ cũ

    // Dòng 2: Trạng thái 5 bình
    lcd->setCursor(0, 1);
    lcd->print("Packs: [");
    for (int i = 0; i < TOTAL_PACKS; i++) {
        if (localState.telemetry.isOnline[i]) lcd->print("O");
        else lcd->print("X");
    }
    lcd->print("]   ");
}

void LCD_Remote_Manager::drawLostConnection() {
    lcd->setCursor(0, 0);
    lcd->print("! SIGNAL LOST ! ");
    lcd->setCursor(0, 1);
    lcd->print("Check Master ESP");
}

// --- THÊM HÀM NÀY: Cơ chế tự động Reset I2C khi bị nhiễu ---
void LCD_Remote_Manager::checkHealth() {
    refreshCounter++;
    // Vòng lặp chạy 200ms/lần. Đếm 10 lần (2 giây) thì rà soát lại I2C.
    if (refreshCounter > 10) {
        Wire.beginTransmission(LCD_ADDR);
        if (Wire.endTransmission() == 0) {
            // Tái khởi động lại chip điều khiển trên LCD để chống lệch pha (4-bit sync)
            lcd->init(); 
        }
        refreshCounter = 0;
    }
}

void LCD_Remote_Manager::loop() {
    while (1) {
        checkHealth();
        
        System_Get_Snapshot(&localState);

        if (localState.lastRecvTime == 0) {
            // Chưa nhận được gói tin nào từ lúc bật máy
        } else if (localState.isConnected) {
            drawDashboard();
        } else {
            drawLostConnection();
        }

        vTaskDelay(pdMS_TO_TICKS(200)); // Refresh màn hình 5 lần/giây
    }
}

void Task_LCD_Run(void *pvParameters) {
    LCD_Remote_Manager myLCD(LCD_ADDR, LCD_COLS, LCD_ROWS);
    myLCD.init();
    myLCD.loop();
}