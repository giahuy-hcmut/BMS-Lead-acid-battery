#include "Task_LCD.h"
#include <Wire.h>

LCD_Remote_Manager::LCD_Remote_Manager(uint8_t addr, uint8_t cols, uint8_t rows) {
    lcd = new LiquidCrystal_I2C(addr, cols, rows);
    refreshCounter = 0;
    currentPage = 0;
    lastPageChange = 0;
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
    char buf[17]; // Bộ đệm chuỗi 16 ký tự

    // Logic lật trang mỗi 2 giây
    if (millis() - lastPageChange > 2000) {
        currentPage++;
        if (currentPage > TOTAL_PACKS) currentPage = 0; // Trang 0 là Tổng quan, Trang 1->5 là chi tiết từng bình
        lastPageChange = millis();
        lcd->clear(); // Xóa màn hình khi đổi trang
    }

    if (currentPage == 0) {
        // --- TRANG TỔNG QUAN (V, A, SOC, Online Status) ---
        // Dòng 1: "60.0V 12.5A 100%"
        sprintf(buf, "%4.1fV %4.1fA %3d%%", localState.telemetry.totalVoltage, localState.telemetry.systemCurrent, localState.telemetry.systemSOC);
        lcd->setCursor(0, 0);
        lcd->print(buf);

        // Dòng 2: "Packs: [OOOOX]"
        lcd->setCursor(0, 1);
        lcd->print("Packs: [");
        for (int i = 0; i < TOTAL_PACKS; i++) {
            lcd->print(localState.telemetry.isOnline[i] ? "O" : "X");
        }
        lcd->print("]");
    } 
    else {
        // --- TRANG CHI TIẾT TỪNG BÌNH ---
        int pIdx = currentPage - 1; // Chỉ số mảng (0 -> 4)
        
        lcd->setCursor(0, 0);
        sprintf(buf, "PACK %d: ", pIdx + 1);
        lcd->print(buf);
        
        if (localState.telemetry.isOnline[pIdx]) {
            // Dòng 1 tiếp tục: "12.0V"
            sprintf(buf, "%4.1fV", localState.telemetry.packVolts[pIdx]);
            lcd->print(buf);
            
            // Dòng 2: "T:35C ERR:0x00"
            lcd->setCursor(0, 1);
            sprintf(buf, "T:%2dC  ERR:0x%02X", localState.telemetry.packTemps[pIdx], localState.telemetry.packStatus[pIdx]);
            lcd->print(buf);
        } else {
            lcd->print("LOST!");
            lcd->setCursor(0, 1);
            lcd->print("Check Connect..");
        }
    }
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