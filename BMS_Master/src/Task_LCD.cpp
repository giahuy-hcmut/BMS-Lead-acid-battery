#include "Task_LCD.h"
#include <Wire.h>

// --- IMPLEMENTATION CỦA CLASS LCD_MANAGER ---

LCD_Manager::LCD_Manager(uint8_t addr, uint8_t cols, uint8_t rows) {
    lcd = new LiquidCrystal_I2C(addr, cols, rows);
    currentPage = 0;
    refreshCounter = 0;
    totalVolt = 0;
    activeCount = 0;
    // Xóa sạch dữ liệu ban đầu
    for(int i=0; i<5; i++) {
        localPacks[i].lastUpdate = 0;
        localPacks[i].voltage = 0;
        localPacks[i].isConnected = false;
    }
}

void LCD_Manager::init() {
    lcd->init(); 
    lcd->backlight();
    
    // Set tốc độ I2C xuống thấp để chống nhiễu
    Wire.setClock(10000); 
    
    lcd->setCursor(0, 0);
    lcd->print("BMS MASTER V2.0");
    lcd->setCursor(0, 1);
    lcd->print("System Init...");
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void LCD_Manager::fetchData() {
    // Thử lấy khóa trong 100ms
    if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
        
        totalVolt = 0;
        activeCount = 0;
        
        for(int i=0; i<5; i++) {
            // 1. Copy dữ liệu thô từ kho chung
            localPacks[i] = globalPacks[i]; 
            
            // 2. [QUAN TRỌNG] Tự tính trạng thái Online tại chỗ
            // (Logic này đảm bảo LCD đồng bộ với Terminal)
            bool isLive = (localPacks[i].lastUpdate > 0) && 
                          (millis() - localPacks[i].lastUpdate < 3000);
            
            localPacks[i].isConnected = isLive;

            // 3. Cộng dồn nếu Online
            if (localPacks[i].isConnected) {
                totalVolt += localPacks[i].voltage;
                activeCount++;
            }
        }
        xSemaphoreGive(dataMutex); // Trả khóa ngay lập tức
    }
}

void LCD_Manager::checkHealth() {
    refreshCounter++;
    // Mỗi 10 chu kỳ (khoảng 30s), kiểm tra và reset LCD nếu cần
    if (refreshCounter > 10) {
        Wire.beginTransmission(0x27);
        if (Wire.endTransmission() == 0) {
            lcd->init();      
            lcd->backlight(); 
        }
        refreshCounter = 0;
    }
}

void LCD_Manager::drawSummary() {
    lcd->setCursor(0, 0);
    lcd->print("TOTAL: "); lcd->print(totalVolt, 1); lcd->print("V");
    
    lcd->setCursor(0, 1);
    // In dạng: "Active: 2/5 Pks"
    lcd->print("Active: "); lcd->print(activeCount); lcd->print("/5 Pks");
}

void LCD_Manager::drawDetail(int packIndex) {
    // Dòng 1: Tên Pack và ID Hex
    lcd->setCursor(0, 0);
    lcd->printf("PACK %d (0x%X)", packIndex + 1, 0x103 + packIndex);

    // Dòng 2: Điện áp hoặc báo lỗi
    lcd->setCursor(0, 1);
    if (localPacks[packIndex].isConnected) {
        lcd->print("Vol: "); 
        lcd->print(localPacks[packIndex].voltage, 2); 
        lcd->print("V");
    } else {
        lcd->print("DISCONNECTED !");
    }
}

void LCD_Manager::loop() {
    while (1) {
        checkHealth(); // Tự sửa lỗi nhiễu
        fetchData();   // Lấy dữ liệu mới
        
        lcd->clear();  // Xóa màn hình
        
        // Logic chuyển trang
        if (currentPage == 0) {
            drawSummary();
        } else {
            drawDetail(currentPage - 1);
        }

        currentPage++;
        if (currentPage > 5) currentPage = 0;
        
        // Dừng 3 giây để người dùng đọc
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// Wrapper cho FreeRTOS
void Task_LCD_Run(void *pvParameters) {
    LCD_Manager myDisplay(0x27, 16, 2);
    myDisplay.init();
    myDisplay.loop();
}