#include "Task_Terminal.h"

// --- IMPLEMENTATION CLASS ---

// 1. Constructor
Terminal_Dashboard::Terminal_Dashboard() {
    // Khởi tạo bộ đệm rỗng để tránh giá trị rác ban đầu
    for(int i=0; i<5; i++) {
        localPacks[i].voltage = 0.0f;
        localPacks[i].isConnected = false;
    }
}

// 2. Init: In màn hình chào mừng 1 lần duy nhất
void Terminal_Dashboard::init() {
    // Lưu ý: Serial.begin đã được gọi ở main.cpp nên không gọi lại ở đây
    Serial.println("\n\n");
    Serial.println("****************************************");
    Serial.println("* BMS MASTER CONSOLE (OOP)        *");
    Serial.println("* System Ready... Starting...     *");
    Serial.println("****************************************");
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// 3. fetchData: Mượn chìa khóa kho để copy dữ liệu về mình
void Terminal_Dashboard::fetchData() {
    if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
        // Copy toàn bộ 5 pack một lúc (Snapshot)
        for(int i=0; i<5; i++) {
            localPacks[i] = globalPacks[i];
            
            // Logic kiểm tra Timeout (tự xử lý tại đây luôn)
            bool isOnline = (localPacks[i].lastUpdate > 0) && 
                            (millis() - localPacks[i].lastUpdate < 3000);
            localPacks[i].isConnected = isOnline;
        }
        xSemaphoreGive(dataMutex); // Trả khóa ngay
    }
}

// 4. printHeader: Vẽ phần đầu bảng
void Terminal_Dashboard::printHeader(uint32_t uptime) {
    // Xóa màn hình (dùng mã ANSI escape code) - tùy chọn, giúp bảng đứng yên
    // Serial.print("\033[2J\033[H"); 
    
    Serial.println("\n"); // Cách dòng cho thoáng
    Serial.println("===========================================");
    Serial.printf("   BMS MASTER DASHBOARD (Up: %lu s)\n", uptime);
    Serial.println("===========================================");
    Serial.println("| ID    | VOLTAGE | STATUS      | UPDATED |");
    Serial.println("|-------|---------|-------------|---------|");
}

// 5. printRow: Vẽ 1 dòng dữ liệu
void Terminal_Dashboard::printRow(int index, BMS_Pack_State &pack) {
    int canID = 0x103 + index;
    
    Serial.printf("| 0x%03X | ", canID);

    if (pack.isConnected) {
        // %6.2f: Chiếm 6 ký tự, 2 số lẻ -> Căn lề rất đẹp
        Serial.printf("%6.2f V | [ONLINE] ✅ | %4lu ms |\n", 
                      pack.voltage, millis() - pack.lastUpdate);
    } else {
        Serial.printf(" --.-- V | [LOST]   ❌ |  ----   |\n");
    }
}

// 6. printFooter: Vẽ đường kết thúc
void Terminal_Dashboard::printFooter() {
    Serial.println("===========================================");
}

// 7. run: Vòng lặp chính (Main Loop của Task)
void Terminal_Dashboard::run() {
    init(); // Chạy 1 lần đầu

    while (1) {
        // Bước 1: Lấy dữ liệu mới nhất
        fetchData();

        // Bước 2: Vẽ bảng
        printHeader(millis() / 1000);
        
        // Vẽ 5 dòng cho 5 pack
        for(int i=0; i<5; i++) {
            printRow(i, localPacks[i]);
        }
        
        printFooter();

        // Bước 3: Nghỉ 1 giây (Refresh rate)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


// --- FREE RTOS WRAPPER ---
void Task_Terminal_Run(void *pvParameters) {
    // Tạo đối tượng Dashboard (Object)
    Terminal_Dashboard myConsole;
    
    // Kích hoạt vòng lặp
    myConsole.run();
}