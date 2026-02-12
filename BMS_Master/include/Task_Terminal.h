#ifndef TASK_TERMINAL_H
#define TASK_TERMINAL_H

#include <Arduino.h>
#include "System_Data.h"

// Class quản lý giao diện Serial Monitor
class Terminal_Dashboard {
private:
    // Bộ nhớ đệm (Cache) để lưu dữ liệu lấy từ kho chung
    BMS_Pack_State localPacks[5];
    
    // Các hàm nội bộ (Helper methods) - chỉ dùng trong class này
    void fetchData();           // Copy dữ liệu an toàn (Mutex)
    void printHeader(uint32_t uptime); // In tiêu đề
    void printRow(int id, BMS_Pack_State &pack); // In 1 dòng dữ liệu
    void printFooter();         // In đường kẻ kết thúc

public:
    // Constructor
    Terminal_Dashboard();

    // Hàm khởi tạo (In logo chào mừng)
    void init();

    // Vòng lặp chính
    void run();
};

// Wrapper cho FreeRTOS gọi
void Task_Terminal_Run(void *pvParameters);

#endif