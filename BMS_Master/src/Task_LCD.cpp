#include "Task_LCD.h"
#include "System_Data.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h> // Thêm thư viện này để điều khiển I2C sâu hơn

// Địa chỉ I2C thường là 0x27 hoặc 0x3F. ESP32 chân mặc định: SDA=21, SCL=22
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// --- HÀM PHỤ: Kiểm tra xem LCD có còn phản hồi không ---
bool Check_LCD_I2C() {
    Wire.beginTransmission(0x27);
    byte error = Wire.endTransmission();
    return (error == 0);
}

void Task_LCD_Run(void *pvParameters) {
    // [FIX NHIỄU]: Giảm tốc độ I2C xuống 10kHz (Mặc định là 100kHz)
    // Giúp tín hiệu đi xa hơn và ổn định hơn trên dây nối lỏng lẻo
    Wire.setClock(10000); 

    // 1. Khởi tạo LCD
    lcd.init();
    lcd.backlight();
    
    // Màn hình chào mừng
    lcd.setCursor(0, 0);
    lcd.print("BMS MASTER V2.0");
    lcd.setCursor(0, 1);
    lcd.print("System Init...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    int page_index = 0; // Biến để lật trang
    int refresh_count = 0; // Biến đếm số lần quét để tự reset

    while (1) {
        // --- CƠ CHẾ TỰ SỬA LỖI (SELF-HEALING) ---
        // Cứ mỗi 10 vòng lặp (khoảng 30 giây), ta khởi tạo lại LCD 1 lần.
        // Điều này giúp xóa các ký tự lạ (garbage) nếu bị nhiễu điện.
        refresh_count++;
        if (refresh_count > 10) {
            // Chỉ reset nếu LCD vẫn đang kết nối dây tốt
            if (Check_LCD_I2C()) {
                lcd.init();      // Khởi tạo lại cấu hình
                lcd.backlight(); // Bật lại đèn nền
            }
            refresh_count = 0;
        }

        // --- BƯỚC A: SAO CHÉP DỮ LIỆU AN TOÀN ---
        // Ta tạo biến tạm để lưu dữ liệu, tránh việc giữ Mutex quá lâu khi đang vẽ LCD
        float volt_p1 = 0, volt_p2 = 0;
        bool conn_p1 = false, conn_p2 = false;
        float total_volt = 0;

        if (xSemaphoreTake(dataMutex, 100) == pdTRUE) {
            // Copy dữ liệu từ kho chung ra biến tạm
            volt_p1 = globalPacks[0].voltage;
            conn_p1 = globalPacks[0].isConnected;
            
            volt_p2 = globalPacks[1].voltage;
            conn_p2 = globalPacks[1].isConnected;
            
            // Tính tổng áp (Ví dụ đơn giản)
            if(conn_p1) total_volt += volt_p1;
            if(conn_p2) total_volt += volt_p2;

            xSemaphoreGive(dataMutex); // Trả khóa ngay!
        }

        // --- BƯỚC B: HIỂN THỊ (Vẽ dựa trên biến tạm) ---
        lcd.clear(); // Xóa màn hình cũ (Xóa luôn cả ký tự rác nếu có)

        if (page_index == 0) {
            // TRANG 1: TỔNG QUAN
            lcd.setCursor(0, 0);
            lcd.print("TOTAL: "); lcd.print(total_volt, 1); lcd.print("V");
            
            lcd.setCursor(0, 1);
            lcd.print("P1:"); lcd.print(conn_p1 ? "OK" : "LOST");
            lcd.print(" P2:"); lcd.print(conn_p2 ? "OK" : "LOST");
        } 
        else if (page_index == 1) {
            // TRANG 2: CHI TIẾT PACK 1 (0x103)
            lcd.setCursor(0, 0);
            lcd.print("PACK 1 (0x103)");
            lcd.setCursor(0, 1);
            if (conn_p1) {
                lcd.print("Vol: "); lcd.print(volt_p1, 2); lcd.print("V");
            } else {
                lcd.print("DISCONNECTED !");
            }
        }
        else if (page_index == 2) {
            // TRANG 3: CHI TIẾT PACK 2 (0x104)
            lcd.setCursor(0, 0);
            lcd.print("PACK 2 (0x104)");
            lcd.setCursor(0, 1);
            if (conn_p2) {
                lcd.print("Vol: "); lcd.print(volt_p2, 2); lcd.print("V");
            } else {
                lcd.print("DISCONNECTED !");
            }
        }

        // --- BƯỚC C: CHUYỂN TRANG ---
        page_index++;
        if (page_index > 2) page_index = 0; // Quay lại trang đầu

        // Giữ màn hình trong 3 giây để người dùng kịp đọc
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}