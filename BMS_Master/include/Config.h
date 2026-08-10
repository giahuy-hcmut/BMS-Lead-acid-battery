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
// Frame master -> slave. Chở dòng điện pack VÀ đóng luôn vai heartbeat: slave
// thức khi nhận được nó, ngủ sau 5 s không thấy. Không có frame heartbeat
// riêng. Cùng tên CAN_MASTER_ID với Board_Config.h bên slave.
#define CAN_MASTER_ID           0x100
#define CAN_MASTER_INTERVAL_MS  5       // 200 Hz - nhịp Predict của Kalman bên slave
#define CAN_TX_TIMEOUT_MS       10      // Timeout twai_transmit (ms)
#define CAN_RX_POLL_TIMEOUT_MS  0       // twai_receive KHÔNG chặn - xem Task_CAN.cpp

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
// Địa chỉ 192.168.4.1
#define REMOTE_MAC_ADDRESS      {0xB8, 0xD6, 0x1A, 0xB8, 0x9C, 0xCC}

// --- 5. CẤU HÌNH WEB SERVER (WIFI AP) ---
#define WIFI_AP_SSID            "MURATA_EV_BMS" // Tên WiFi phát ra
#define WIFI_AP_PASS            "12345678"      // Mật khẩu WiFi (ít nhất 8 ký tự)
#define WIFI_AP_CHANNEL         1               // BẮT BUỘC LÀ 1 ĐỂ KHÔNG CHẾT ESP-NOW
#define WEB_UPDATE_INTERVAL     500             // Tốc độ làm mới Web (ms)

// --- 6. CẤU HÌNH CẢM BIẾN DÒNG ---
// SOC KHÔNG còn tính ở master: mỗi slave chạy Kalman rồi gửi về ở byte 4 của
// frame CAN, master lấy min qua System_MinSoc(). Các hằng số phục vụ bộ đếm
// Coulomb cũ (BATTERY_CAPACITY_AH, VOLTAGE_SYS_*) đã xoá cùng bộ đếm đó.
//
// TODO: ba hằng số dưới còn neo theo cảm biến Hall ACS712/ACS758. Phần cứng
// đã chốt đổi sang SHUNT + amp (INA240/INA282) nên phải suy lại sensitivity và
// zero theo giá trị shunt (mΩ) × hệ số khuếch đại. Chưa đổi thì dòng đọc sai.
#define PIN_CURRENT_SENSOR      32
#define ACS758_SENSITIVITY      0.0264  // Độ nhạy 26.4mV/A khi cấp nguồn 3.3V
#define ACS758_ZERO_VOLTAGE     0//1.524    // Điện áp khi dòng = 0A (3.3V / 2)
#define ACS758_ZERO_CURRENT     0.5     //  Dòng điện để calib khử từ trường

// --- 7. CẤU HÌNH BẢO VỆ & ĐIỀU KHIỂN RELAY ---
#define PIN_RELAY_CONTROL       26      // Chân xuất tín hiệu điều khiển Relay tổng
// Lưu ý: Đa số Module Relay cách ly quang (Opto) kích ở mức THẤP (LOW). 
// Nếu module của bạn kích mức CAO, hãy đảo ngược lại định nghĩa này.
#define RELAY_ON                HIGH    
#define RELAY_OFF               LOW     

// Ngưỡng quá dòng (A) - ngắt relay khi vượt.
// 50 A là số cũ từ hồi giả định xe rút 50 A. Thực tế là 4 motor BLDC x 25 A
// = ~100 A đỉnh, nên 50 A sẽ NGẮT RELAY GIỮA LÚC TĂNG TỐC BÌNH THƯỜNG.
// 150 A = 1.5x đỉnh dự kiến, và 65% của Max Discharge Current 230 A trong
// datasheet CSB EVX12200 -> vừa không ngắt oan, vừa dưới giới hạn của bình.
// TODO xác nhận lại theo giới hạn dòng của bộ điều khiển motor khi có số.
#define MAX_DISCHARGE_CURRENT   150.0f
#define MIN_SOC_SHUTDOWN        5       // Mức % SOC thấp nhất cho phép chạy (Bảo vệ cạn bình)
#define RECOVERY_SOC            10      // Mức % SOC an toàn để tự động đóng Relay trở lại (Hysteresis)

#endif /* CONFIG_H */