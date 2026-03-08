#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- 1. CẤU HÌNH HỆ THỐNG ---
#define TOTAL_PACKS             2       // Phải khớp với Master
#define CONNECTION_TIMEOUT      3000    // Nếu 3 giây không có sóng -> Báo mất kết nối

// --- 2. CẤU HÌNH MÀN HÌNH LCD ---
#define PIN_I2C_SDA             21
#define PIN_I2C_SCL             22
#define LCD_ADDR                0x27    // Địa chỉ I2C của LCD
#define LCD_COLS                16
#define LCD_ROWS                2

#endif