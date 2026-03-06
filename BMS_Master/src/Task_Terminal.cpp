#include "Task_Terminal.h"

Terminal_Dashboard::Terminal_Dashboard() {
    for(int i=0; i<TOTAL_PACKS; i++) {
        localPacks[i].voltage = 0.0f;
        localPacks[i].isConnected = false;
        localPacks[i].current = 0.0f;
        localPacks[i].soc = 0;
    }
}

void Terminal_Dashboard::init() {
    Serial.println("\n\n");
    Serial.println(">>> BMS MASTER CONSOLE V3.0 <<<");
    Serial.println(">>> TEST MODE: 2 PACKS LITHIUM <<<");
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void Terminal_Dashboard::fetchData() {
    // [CHUẨN CÔNG NGHIỆP] Lấy bản chụp an toàn từ kho dữ liệu hệ thống
    System_Get_Snapshot(localPacks);

    // Tính trạng thái Online dựa trên thời gian cập nhật cuối cùng
    for(int i=0; i<TOTAL_PACKS; i++) {
        bool isOnline = (localPacks[i].lastUpdate > 0) && 
                        (millis() - localPacks[i].lastUpdate < LCD_TIMEOUT);
        localPacks[i].isConnected = isOnline;
    }
}

void Terminal_Dashboard::printHeader(uint32_t uptime) {
    float totalVolt = 0;
    // Tính tổng điện áp của các Pack đang Online để kiểm tra OCV
    for(int i=0; i<TOTAL_PACKS; i++) {
        if(localPacks[i].isConnected) totalVolt += localPacks[i].voltage;
    }

    Serial.println("\n==========================================================");
    Serial.printf("   BMS MASTER DASHBOARD (Uptime: %lu s)\n", uptime);
    Serial.println("==========================================================");
    
    // --- IN THÔNG SỐ TỔNG (QUAN TRỌNG ĐỂ TEST SOC) ---
    // localPacks[0].current và localPacks[0].soc chứa dữ liệu tổng của hệ thống
    Serial.printf(" SYSTEM VOLTAGE: %6.2f V  |  CURRENT: %6.2f A\n", totalVolt, localPacks[0].current);
    Serial.printf(" SYSTEM SOC    : %3d %%       |  MODE   : TEST (2S)\n", localPacks[0].soc);
    
    Serial.println("----------------------------------------------------------");
    Serial.println("| ID    | VOLTAGE | TEMP  | ERR  | STATUS      | UPDATED |");
    Serial.println("|-------|---------|-------|------|-------------|---------|");
}

void Terminal_Dashboard::printRow(int index, BMS_Pack_State &pack) {
    int canID = CAN_BASE_ID + index;
    Serial.printf("| 0x%03X | ", canID);
    
    if (pack.isConnected) {
        // In nhiệt độ và mã lỗi nhận từ Slave qua mạng CAN
        Serial.printf("%6.2f V | %3d C | 0x%02X | [ONLINE] ✅ | %4lu ms |\n", 
                      pack.voltage, pack.temperature, pack.status, millis() - pack.lastUpdate);
    } else {
        Serial.printf(" --.-- V |  -- C | ---- | [LOST]   ❌ |  ----   |\n");
    }
}

void Terminal_Dashboard::printFooter() {
    Serial.println("==========================================================");
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
        // Cập nhật màn hình mỗi 1 giây
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void Task_Terminal_Run(void *pvParameters) {
    Terminal_Dashboard dashboard;
    dashboard.run();
}