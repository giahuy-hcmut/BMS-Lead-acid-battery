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

// Nhịp quét bảo vệ (Task_Logic). 10ms: quá dòng phát hiện <=10ms, vẫn nhường CPU.
#define PROTECTION_PERIOD_MS    10

// --- 3. TIMEOUT SLAVE ---
// Không nhận frame của một slave trong khoảng này -> coi là mất kết nối
// (System_Get_Snapshot() đặt isConnected = false, Task_Logic ngắt relay).
// Tên cũ là LCD_TIMEOUT, nhưng nó chưa bao giờ liên quan gì tới LCD: đây là
// timeout của CAN, và System_Data.cpp mới là chỗ dùng chính.
// LCD đã bỏ, giải phóng GPIO 21/22 (I2C) cho MCP2515.
#define SLAVE_TIMEOUT_MS        3000

// --- 4. CẤU HÌNH ESP-NOW ---
// Địa chỉ MAC của Tay Cầm (B8:D6:1A:B8:9C:CC)
// Địa chỉ 192.168.4.1
#define REMOTE_MAC_ADDRESS      {0xB8, 0xD6, 0x1A, 0xB8, 0x9C, 0xCC}

// --- 5. CẤU HÌNH WEB SERVER (WIFI AP) ---
#define WIFI_AP_SSID            "MURATA_EV_BMS" // Tên WiFi phát ra
#define WIFI_AP_PASS            "12345678"      // Mật khẩu WiFi (ít nhất 8 ký tự)
#define WIFI_AP_CHANNEL         1               // BẮT BUỘC LÀ 1 ĐỂ KHÔNG CHẾT ESP-NOW
#define WEB_UPDATE_INTERVAL     500             // Tốc độ làm mới Web (ms)

// --- 6. CẢM BIẾN DÒNG: INA219 + SHUNT NGOÀI ---
// SOC KHÔNG còn tính ở master: mỗi slave chạy Kalman rồi gửi về ở byte 4 của
// frame CAN, master lấy min qua System_MinSoc(). Các hằng số phục vụ bộ đếm
// Coulomb cũ (BATTERY_CAPACITY_AH, VOLTAGE_SYS_*) đã xoá cùng bộ đếm đó.
//
// Shunt 100 A / 75 mV = 0.75 mOhm, class 0.5. R100 (0.1 Ohm) trên module ĐÃ GỠ:
// để lại thì nó nằm song song 2 dây sense và rút 0.75 A qua dây dành cho microvolt.
//
// Đo LOW-SIDE ở cực âm pack. Common-mode của INA219 chỉ 0-26 V, không chịu được
// 60 V (72-75 V lúc sạc) của high-side. GND của ESP32 bám vào đầu IN- (phía cực
// âm pack) nên chiều XẢ nằm trong dải hợp lệ; chiều regen ra ngoài spec (-56 mV
// ở 75 A) nhưng vẫn trong absolute max -0.3 V. Ghi rõ giới hạn này trong báo cáo.
//
// Số ĐO THẬT trên bench (Code/BENCH_INA219, 2026-08-12) - không phải số datasheet:
//   thang đo  320 mV -> +/-427 A   (kẹp ở đúng 320.000 khi cấp 3.27 V vi sai)
//   offset    -17.5 uV = -23.3 mA  -> 0.117 %/h trôi Coulomb trên 20 Ah
//   nhiễu     27.2 uV 1 mẫu        -> 12.8 mA sau lọc 8 mẫu
//   bước      10 uV = 13.3 mA
//   gain      khớp vôn kế trong 1 % (datasheet ±1 %, shunt class 0.5 -> tổng ±1.5 %)
#define INA219_I2C_ADDR         0x40
#define PIN_INA_SDA             21
#define PIN_INA_SCL             22

#define SHUNT_FULL_A            100.0f
#define SHUNT_FULL_MV           75.0f

// Đo ở bước 5 của bench, SAU khi hàn shunt (gồm cả nhiệt điện động mối nối: hàn
// vào chỉ làm offset dịch 6 uV, nên mối hàn sạch).
#define CURRENT_ZERO_MV         (-0.0175f)

// +1 = dòng XẢ ra số DƯƠNG. Đặt theo cách đấu dây: IN+ ở đầu mà dòng ĐI VÀO shunt
// khi xả (phía điểm mass sao).
//
// CHƯA xác nhận bằng dòng thật - không có tải trên bàn. PHẢI kiểm ở lần chạy xe
// đầu tiên: SOC phải GIẢM, byte 5 của 0x200 phải = 2 (DISCHARGE), dòng trên web
// phải DƯƠNG. Sai thì đổi thành -1.0f, KHÔNG tháo dây.
//
// Dấu sai là NGUY HIỂM: packI âm làm (packI > MAX_DISCHARGE_CURRENT) luôn sai
// -> bảo vệ quá dòng tắt hoàn toàn. Bù lại nó rất ồn ào (SOC tăng khi đang chạy)
// nên hoãn được, miễn là nằm trong checklist khởi động.
#define CURRENT_SIGN            (+1.0f)

// Vùng chết quanh 0: dưới ngưỡng này Task_Current ép dòng về đúng 0.0. 0.5 A là
// ~21x nhiễu đo được (12.8 mA sau lọc) nên nó không cắt mất tín hiệu thật.
// Task_VehicleCAN dùng lại CHÍNH hằng này làm ranh giới "đang nghỉ" - một con số,
// hai chỗ đọc, không có cơ hội lệch nhau.
#define CURRENT_IDLE_BAND       0.5f
#define TASK_CURRENT_PERIOD_MS  3       // 8 mẫu x 3 ms = cửa sổ lọc 24 ms
#define CURRENT_FAULT_LIMIT     20      // lần I2C không ACK LIÊN TIẾP -> báo mất

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

// --- 8. CẤU HÌNH CAN XE (MCP2515 qua SPI) ---
// ESP32 chỉ có MỘT bộ TWAI và nó đã cõng bus nội bộ master<->slave, nên bus xe
// bắt buộc cần controller CAN thứ hai. Module HW-184: MCP2515 + TJA1050.
//
// Dùng chân VSPI MẶC ĐỊNH nên KHÔNG phải gọi SPI.begin() với chân tuỳ chọn -
// bớt một chỗ sai, và khớp mọi ví dụ của thư viện:
//     SCK = 18 · MISO = 19 · MOSI = 23
#define PIN_VCAN_CS             5
#define PIN_VCAN_INT            4       // CHƯA NỐI, CHƯA DÙNG - hiện chỉ GỬI.
                                        // Dời từ 22 về 4: GPIO 22 là chân SCL mặc
                                        // định của I2C và INA219 dùng nó THẬT, còn
                                        // chân này chưa nối dây. Chân đang dùng
                                        // thắng chân dự trữ.
#define VCAN_STATUS_ID          0x200   // BMS -> xe: số liệu hệ thống
#define VCAN_PACK_ID            0x201   // BMS -> xe: từng bình, ghép kênh
#define VCAN_PERIOD_MS          100
#define VCAN_TX_FAIL_LIMIT      10      // thất bại LIÊN TIẾP -> re-init

// Bật để bring-up KHÔNG cần bus nào: MCP2515 tự nhận lại frame mình gửi.
//#define VCAN_LOOPBACK

#endif /* CONFIG_H */