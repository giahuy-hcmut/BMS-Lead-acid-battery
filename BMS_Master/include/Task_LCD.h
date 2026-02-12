#ifndef TASK_LCD_H
#define TASK_LCD_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "System_Data.h"

// Class quản lý việc hiển thị
class LCD_Manager {
private:
    LiquidCrystal_I2C* lcd; // Con trỏ tới đối tượng LCD
    
    // Dữ liệu nội bộ (Cache)
    BMS_Pack_State localPacks[5];
    float totalVolt;
    int activeCount;
    
    // Biến trạng thái
    int currentPage;
    int refreshCounter;

    // Các hàm nội bộ (Private methods)
    void fetchData();       // Copy dữ liệu từ kho chung (Mutex)
    void checkHealth();     // Tự sửa lỗi (Self-healing)
    void drawSummary();     // Vẽ trang tổng quan
    void drawDetail(int packIndex); // Vẽ trang chi tiết
    
public:
    // Constructor
    LCD_Manager(uint8_t addr, uint8_t cols, uint8_t rows);
    
    // Hàm khởi tạo phần cứng
    void init();
    
    // Hàm chạy chính (Vòng lặp)
    void loop();
};

// Hàm wrapper để FreeRTOS gọi được (C-style interface)
void Task_LCD_Run(void *pvParameters);

#endif