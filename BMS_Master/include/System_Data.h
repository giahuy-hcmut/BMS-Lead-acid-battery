#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include "Config.h" 

// ==========================================
// [3. CẤU TRÚC DỮ LIỆU]
// ==========================================
//Cấu trúc lưu trữ chính
struct BMS_Pack_State {
    float voltage;
    float current;
    int soc;
    int8_t temperature;    // Bổ sung: Lưu nhiệt độ thực tế (đã trừ 40)
    uint8_t status;        // Bổ sung: Lưu mã lỗi (0x00 là bình thường)
    uint32_t lastUpdate;
    bool isConnected;
};

//Dùng cho task_CAN thu thập dữ liệu
typedef struct {
    uint32_t can_id;
    float voltage;
    int8_t temperature;    // <--- THÊM DÒNG NÀY (Sửa lỗi cho Task_CAN và Task_Logic)
    uint8_t status;        // <--- THÊM DÒNG NÀY (Sửa lỗi cho Task_CAN và Task_Logic)
    uint8_t soc;           // SOC (%) tu bo loc Kalman tren slave - byte 4
} BMS_Message_t;

// [ĐÃ THAY ĐỔI: Đưa struct ESP-NOW vào đây, dùng Macro TOTAL_PACKS để tránh lỗi khi đổi số bình]
typedef struct __attribute__((packed)) {
    float totalVoltage;     // 4 bytes
    float systemCurrent;    // 4 bytes
    int   systemSOC;        // 4 bytes
    float packVolts[TOTAL_PACKS];     // 20 bytes (5 packs)
    int8_t packTemps[TOTAL_PACKS];    // 5 bytes
    uint8_t packStatus[TOTAL_PACKS];  // 5 bytes
    bool  isOnline[TOTAL_PACKS];      // 5 bytes
} BMS_Telemetry_Packet;     // Tổng cộng: 47 bytes

// Biến toàn cục (Chỉ khai báo extern, không dùng trực tiếp ở các Task)
extern BMS_Pack_State globalPacks[TOTAL_PACKS]; 
extern SemaphoreHandle_t dataMutex;   
extern QueueHandle_t canQueue;        

// Web control flags (set by Task_WebServer, read by Task_Logic / Task_CAN)
extern volatile bool webForceRelayOff;
extern volatile bool webSlavesActive;

// Protection state (set by Task_Logic, read by Task_WebServer)
extern volatile bool systemLocked;
extern char faultReason[64];

// API Hệ thống
void System_Data_Init();
// Sửa lại khai báo hàm ở cuối file
void System_Update_Pack(uint32_t can_id, float voltage, int8_t temp,
                        uint8_t status, uint8_t soc);
void System_Update_Current(float current);
void System_Get_Snapshot(BMS_Pack_State *snapshotArray);

// Dòng pack, dạng scalar. Nhẹ và KHÔNG CHẶN - dùng cho đường phát frame 5 ms.
// System_Get_Snapshot() khoá mutex rồi lặp cả 5 pack, quá nặng cho đường đó.
float System_Get_Current(void);

// Số bình đang giám sát. Hôm nay = TOTAL_PACKS.
// Móc treo cho tính năng "nhập số pack trên web": khi làm, chỉ đổi thân hàm này
// + thêm setter, không phải rà lại các vòng lặp rải khắp 6 file.
uint8_t System_GetPackCount(void);

// SOC hệ thống = SOC NHỎ NHẤT trong các bình.
//
// Pack nối tiếp: bình YẾU NHẤT quyết định giới hạn xả, nên lấy globalPacks[0]
// làm đại diện là sai nếu bình 0 không phải bình yếu nhất.
//
// Trả -1 nếu CÓ BẤT KỲ bình nào offline: không thể biết bình mất tích có phải
// bình yếu nhất hay không, nên báo "không biết" thay vì đoán lạc quan. Người
// gọi phải xử lý -1 (hiện "--", và Task_Logic không mở lại relay).
//
// Hàm THUẦN: không khoá mutex, nhận sẵn snapshot mà người gọi đã lấy.
int System_MinSoc(const BMS_Pack_State *snaps);

// Tổng điện áp các bình ĐANG online (V).
//
// Bỏ qua bình offline: giá trị của chúng là số cũ đọng lại, cộng vào sẽ ra tổng
// cao giả. Task_LCD (đã xoá) từng quên kiểm isConnected và báo tổng khác Web -
// đúng hậu quả của việc mỗi task tự viết lại cùng một vòng lặp. Gom về một chỗ
// để chuyện đó không lặp lại khi thêm task mới.
//
// Hàm THUẦN, cùng khuôn System_MinSoc().
float System_TotalVoltage(const BMS_Pack_State *snaps);

#endif