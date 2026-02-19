#include "Task_Terminal.h"

Terminal_Remote_Dashboard::Terminal_Remote_Dashboard() {}

void Terminal_Remote_Dashboard::init() {
    Serial.println("\n\n>>> MURATA EV: STEERING WHEEL CONSOLE <<<");
}

void Terminal_Remote_Dashboard::printDashboard() {
    Serial.println("\n===========================================");
    if (!localState.isConnected) {
        Serial.println("   [ ⚠️ WARNING: SIGNAL LOST ! ]");
        Serial.println("===========================================");
        return;
    }

    Serial.printf("   TELEMETRY (Delay: %lu ms)\n", millis() - localState.lastRecvTime);
    Serial.println("===========================================");
    Serial.printf(" [⚡] TOTAL VOLT : %6.2f V\n", localState.telemetry.totalVoltage);
    Serial.printf(" [⚡] CURRENT    : %6.2f A\n", localState.telemetry.systemCurrent);
    Serial.println("-------------------------------------------");
    
    Serial.print(" [🔋] PACK STATUS: ");
    for(int i=0; i<TOTAL_PACKS; i++) {
        if (localState.telemetry.isOnline[i]) {
            Serial.printf("[%.1fV] ", localState.telemetry.packVolts[i]);
        } else {
            Serial.print("[❌] ");
        }
    }
    Serial.println("\n===========================================");
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