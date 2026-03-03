#include "Task_Terminal.h"

Terminal_Dashboard::Terminal_Dashboard() {
    for(int i=0; i<TOTAL_PACKS; i++) {
        localPacks[i].voltage = 0.0f;
        localPacks[i].isConnected = false;
    }
}

void Terminal_Dashboard::init() {
    Serial.println("\n\n");
    Serial.println(">>> BMS MASTER CONSOLE V3.0 <<<");
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void Terminal_Dashboard::fetchData() {
    // [CHUẨN CÔNG NGHIỆP] Lấy bản chụp an toàn
    System_Get_Snapshot(localPacks);

    // Tính trạng thái trên bản sao
    for(int i=0; i<TOTAL_PACKS; i++) {
        bool isOnline = (localPacks[i].lastUpdate > 0) && 
                        (millis() - localPacks[i].lastUpdate < LCD_TIMEOUT);
        localPacks[i].isConnected = isOnline;
    }
}

// (Các hàm in ấn giữ nguyên logic hiển thị)
void Terminal_Dashboard::printHeader(uint32_t uptime) {
    Serial.println("\n==========================================================");
    Serial.printf("   BMS MASTER DASHBOARD (Up: %lu s)\n", uptime);
    Serial.println("==========================================================");
    // Thêm cột TEMP và ERR vào tiêu đề
    Serial.println("| ID    | VOLTAGE | TEMP  | ERR  | STATUS      | UPDATED |");
    Serial.println("|-------|---------|-------|------|-------------|---------|");
}

void Terminal_Dashboard::printRow(int index, BMS_Pack_State &pack) {
    int canID = CAN_BASE_ID + index;
    Serial.printf("| 0x%03X | ", canID);
    
    if (pack.isConnected) {
        // In thêm pack.temperature và pack.status
        Serial.printf("%6.2f V | %3d C | 0x%02X | [ONLINE] ✅ | %4lu ms |\n", 
                      pack.voltage, pack.temperature, pack.status, millis() - pack.lastUpdate);
    } else {
        // Nếu mất kết nối thì in dấu gạch ngang
        Serial.printf(" --.-- V |  -- C | ---- | [LOST]   ❌ |  ----   |\n");
    }
}

void Terminal_Dashboard::printFooter() {
    Serial.println("===========================================");
}

void Terminal_Dashboard::run() {
    init();
    while (1) {
        fetchData();
        printHeader(millis() / 1000);
        for(int i=0; i<TOTAL_PACKS; i++) {
            printRow(i, localPacks[i]);
        }
        printFooter();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void Task_Terminal_Run(void *pvParameters) {
    Terminal_Dashboard dashboard;
    dashboard.run();
}