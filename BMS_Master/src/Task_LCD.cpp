#include "Task_LCD.h"
#include "System_Data.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h> 

// Địa chỉ I2C: 0x27 hoặc 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// --- HÀM CHECK I2C (Giữ nguyên để chống treo) ---
bool Check_LCD_I2C() {
    Wire.beginTransmission(0x27);
    byte error = Wire.endTransmission();
    return (error == 0);
}

void Task_LCD_Run(void *pvParameters) {
    // [QUAN TRỌNG] Giảm tốc độ I2C để chống nhiễu trên dây dài
    Wire.setClock(10000); 

    lcd.init();
    lcd.backlight();
    
    // Intro
    lcd.setCursor(0, 0);
    lcd.print("BMS MASTER 5-PACK");
    lcd.setCursor(0, 1);
    lcd.print("System Init...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    int page_index = 0; 
    int refresh_count = 0; 

    // Tạo bộ đệm cục bộ để copy dữ liệu từ kho chung ra
    BMS_Pack_State localPacks[5]; 

    while (1) {
        // --- 1. CƠ CHẾ TỰ SỬA LỖI (SELF-HEALING) ---
        refresh_count++;
        if (refresh_count > 10) {
            if (Check_LCD_I2C()) {
                lcd.init();      
                lcd.backlight(); 
            }
            refresh_count = 0;
        }

        // --- 2. SAO CHÉP DỮ LIỆU (COPY SNAPSHOT) ---
        // Tính toán tổng áp ngay lúc copy để tiết kiệm thời gian hiển thị
        float total_volt = 0;
        int active_packs = 0;

        if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
            // Dùng vòng lặp copy cả 5 Pack
            for(int i=0; i<5; i++) {
                localPacks[i] = globalPacks[i]; // Copy struct
                
                // Nếu Pack này đang kết nối, cộng dồn vào tổng
                if (localPacks[i].isConnected) {
                    total_volt += localPacks[i].voltage;
                    active_packs++;
                }
            }
            xSemaphoreGive(dataMutex); 
        }

        // --- 3. HIỂN THỊ (DYNAMIC) ---
        lcd.clear(); 

        if (page_index == 0) {
            // --- TRANG TỔNG QUAN ---
            lcd.setCursor(0, 0);
            lcd.print("TOTAL: "); 
            lcd.print(total_volt, 1); // 1 số lẻ
            lcd.print("V");
            
            lcd.setCursor(0, 1);
            lcd.print("Active: "); 
            lcd.print(active_packs); 
            lcd.print("/5 Pks"); // Ví dụ: "Active: 2/5 Pks"
        } 
        else {
            // --- TRANG CHI TIẾT (1 -> 5) ---
            // page_index = 1 -> Hiển thị Pack 0 (ID 0x103)
            // page_index = 2 -> Hiển thị Pack 1 (ID 0x104)
            int pack_id = page_index - 1; 

            lcd.setCursor(0, 0);
            // In ID dạng Hex cho ngầu: P1(103), P2(104)...
            lcd.printf("PACK %d (0x%X)", pack_id + 1, 0x103 + pack_id);

            lcd.setCursor(0, 1);
            if (localPacks[pack_id].isConnected) {
                lcd.print("Vol: "); 
                lcd.print(localPacks[pack_id].voltage, 2); 
                lcd.print("V");
            } else {
                lcd.print("DISCONNECTED !");
            }
        }

        // --- 4. CHUYỂN TRANG ---
        page_index++;
        // Tổng cộng 6 trang (0: Tổng, 1-5: Chi tiết 5 pack)
        if (page_index > 5) page_index = 0; 

        // Thời gian dừng mỗi trang (3 giây)
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}