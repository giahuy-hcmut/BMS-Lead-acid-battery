#include "Task_Terminal.h"

Terminal_Remote_Dashboard::Terminal_Remote_Dashboard() {}

void Terminal_Remote_Dashboard::init() {
    Serial.println("\n\n>>> MURATA EV: STEERING WHEEL CONSOLE <<<");
}

void Terminal_Remote_Dashboard::printDashboard() {
    Serial.println("\n==========================================================");
    if (!localState.isConnected) {
        Serial.println("   [ ⚠️ WARNING: SIGNAL LOST ! ]");
        Serial.println("==========================================================");
        return;
    }

    // Header y hệt Master
    Serial.printf(" SYSTEM VOLTAGE: %6.2f V  |  CURRENT: %6.2f A\n", localState.telemetry.totalVoltage, localState.telemetry.systemCurrent);
    Serial.printf(" SYSTEM SOC    : %3d %%       |  LAG    : %lu ms\n", localState.telemetry.systemSOC, millis() - localState.lastRecvTime);
    
    Serial.println("----------------------------------------------------------");
    Serial.println("| ID    | VOLTAGE | TEMP  | ERR  | STATUS      |         |");
    Serial.println("|-------|---------|-------|------|-------------|---------|");
    
    // In chi tiết từng bình ắc quy
    for(int i=0; i<TOTAL_PACKS; i++) {
        int canID = 0x103 + i; // ID giả lập theo CAN_BASE_ID
        Serial.printf("| 0x%03X | ", canID);
        
        if (localState.telemetry.isOnline[i]) {
            Serial.printf("%6.2f V | %3d C | 0x%02X | [ONLINE] ✅ |         |\n", 
                          localState.telemetry.packVolts[i], 
                          localState.telemetry.packTemps[i], 
                          localState.telemetry.packStatus[i]);
        } else {
            Serial.printf(" --.-- V |  -- C | ---- | [LOST]   ❌ |         |\n");
        }
    }
    Serial.println("==========================================================");
}

void Terminal_Remote_Dashboard::loop() {
    while (1) {
        System_Get_Snapshot(&localState);
        
        // Chỉ in ra monitor nếu đã từng nhận được ít nhất 1 gói tin
        if (localState.lastRecvTime > 0) {
            printDashboard();
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Cập nhật Terminal 1 giây/lần
    }
}

void Task_Terminal_Run(void *pvParameters) {
    Terminal_Remote_Dashboard terminal;
    terminal.init();
    terminal.loop();
}