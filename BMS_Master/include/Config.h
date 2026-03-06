#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- 1. CẤU HÌNH HỆ THỐNG PIN ---
#define TOTAL_PACKS             2       // Số lượng bình ắc quy (Sửa số này nếu nâng cấp xe)
#define CAN_BASE_ID             0x103   // ID bắt đầu của Pack 1

// --- 2. CẤU HÌNH CAN BUS ---
#define PIN_CAN_TX              GPIO_NUM_16
#define PIN_CAN_RX              GPIO_NUM_17
#define CAN_BAUD_RATE           500000
#define CAN_QUEUE_LENGTH        20      // Chiều dài bộ đệm tin nhắn CAN

// --- 3. CẤU HÌNH MÀN HÌNH LCD ---
#define PIN_I2C_SDA             21
#define PIN_I2C_SCL             22
#define LCD_ADDR                0x27    // Địa chỉ I2C của LCD
#define LCD_COLS                16      // Số cột LCD
#define LCD_ROWS                2       // Số hàng LCD
#define LCD_I2C_RECOVERY_TICKS  10      // Số vòng lặp trước khi reset I2C chống nhiễu
#define LCD_TIMEOUT             3000    // Thời gian (ms) báo mất kết nối Slave

// --- 4. CẤU HÌNH ESP-NOW ---
// Địa chỉ MAC của Tay Cầm (B8:D6:1A:B8:9C:CC)
#define REMOTE_MAC_ADDRESS      {0xB8, 0xD6, 0x1A, 0xB8, 0x9C, 0xCC}

// --- 5. CẤU HÌNH WEB SERVER (WIFI AP) ---
#define WIFI_AP_SSID            "MURATA_EV_BMS" // Tên WiFi phát ra
#define WIFI_AP_PASS            "12345678"      // Mật khẩu WiFi (ít nhất 8 ký tự)
#define WIFI_AP_CHANNEL         1               // BẮT BUỘC LÀ 1 ĐỂ KHÔNG CHẾT ESP-NOW
#define WEB_UPDATE_INTERVAL     500             // Tốc độ làm mới Web (ms)

// --- 6. CẤU HÌNH CẢM BIẾN DÒNG & SOC (ACS712-30A) ---
#define PIN_CURRENT_SENSOR      32
#define ACS758_SENSITIVITY      0.0264  // Độ nhạy 26.4mV/A khi cấp nguồn 3.3V
#define ACS758_ZERO_VOLTAGE     1.524    // Điện áp khi dòng = 0A (3.3V / 2)
#define ACS758_ZERO_CURRENT     0.5     //  Dòng điện để calib khử từ trường
#define BATTERY_CAPACITY_AH     2.0     // Dung lượng pin Testbench (2.0 Ah)

// --- 7. CẤU HÌNH NGƯỠNG ĐIỆN ÁP & SOC (Hệ 60V Chì-Axit) ---
#define VOLTAGE_SYS_MIN_VALID   5.0f        //10.0f   // Điện áp tối thiểu để xác nhận CAN đã gửi dữ liệu
#define VOLTAGE_SYS_100_SOC     12.6f        //64.0f   // Điện áp khi bình đầy 100% (Khoảng 12.8V/bình x 5)
#define VOLTAGE_SYS_0_SOC       9.0f        //57.5f   // Điện áp khi bình cạn 0% (Khoảng 11.5V/bình x 5)
#endif