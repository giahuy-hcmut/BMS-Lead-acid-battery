#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- 1. CẤU HÌNH HỆ THỐNG PIN ---
#define TOTAL_PACKS             5       // Số lượng bình ắc quy (Sửa số này nếu nâng cấp xe)
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

// --- 5. CẤU HÌNH CẢM BIẾN DÒNG (Chuẩn bị cho bước sau) ---
#define PIN_CURRENT_SENSOR      GPIO_NUM_36 

#endif