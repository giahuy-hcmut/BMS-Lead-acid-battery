#ifndef TASK_VEHICLE_CAN_H
#define TASK_VEHICLE_CAN_H

#include <Arduino.h>
#include <mcp_can.h>
#include "System_Data.h"

// ==========================================================
//  GIAO THỨC BMS -> XE
// ==========================================================
// Quy ước GIỐNG bus nội bộ, không tạo cái thứ hai:
//     MSB trước · int16 bù 2 · dòng > 0 = XẢ
//
// Cả hai frame mang ĐÚNG tập dữ liệu mà web dashboard đang hiện, nên hai đường
// ra kiểm chéo nhau được bằng mắt.
//
// ---- VCAN_STATUS_ID (0x200) · DLC 8 · chu kỳ VCAN_PERIOD_MS ----
//   byte 0-1  Tổng áp các bình ONLINE   uint16  0.01 V
//   byte 2-3  Dòng pack                 int16   0.01 A   (>0 = xả)
//                                       VCAN_CURRENT_INVALID nếu mất cảm biến
//   byte 4    SOC hệ thống              uint8   1 %      VCAN_SOC_INVALID
//   byte 5    Trạng thái                uint8   VCAN_State_t
//   byte 6    Cờ lỗi                    uint8   bitmask VCAN_FLAG_*
//   byte 7    Bộ đếm sống               uint8   tăng mỗi frame, quay vòng
//
// ---- VCAN_PACK_ID (0x201) · DLC 8 · GHÉP KÊNH, mỗi chu kỳ một bình ----
//   byte 0    Chỉ số bình (bộ chọn)     uint8   0..count-1
//   byte 1-2  Áp bình                   uint16  0.01 V
//   byte 3    SOC bình (Kalman/slave)   uint8   1 %      VCAN_SOC_INVALID
//   byte 4    Nhiệt bình                int8    1 °C     VCAN_TEMP_INVALID
//   byte 5    Cờ lỗi bình               uint8   = byte 6 frame của slave
//   byte 6    Online                    uint8   0 / 1
//   byte 7    Số bình đang giám sát     uint8   System_GetPackCount()
//
// 5 bình -> quét đủ trong 5 chu kỳ = 500 ms. Áp và nhiệt bình đổi rất chậm nên
// đủ nhanh; xe cần số liệu tức thời thì đọc 0x200.
//
// Byte 7 của 0x201 làm frame TỰ MÔ TẢ: xe biết phải chờ bao nhiêu chỉ số thay vì
// hardcode 5. Khớp luôn tính năng "nhập số pack trên web".
//
// VÌ SAO có giá trị INVALID: System_MinSoc() trả -1 khi có slave offline, vì
// không thể biết bình mất tích có phải bình yếu nhất hay không. Gửi một con số
// TRÔNG hợp lý lúc không có dữ liệu nguy hiểm hơn là báo thẳng "không biết".
//
// VÌ SAO có bộ đếm sống: bên nhận kiểm nó có TĂNG không. Frame vẫn tới mà bộ đếm
// đứng nghĩa là BMS treo - timeout frame KHÔNG bắt được ca đó.

typedef enum {
    VCAN_STATE_INIT      = 0,   // chưa từng thấy đủ mọi bình, số liệu chưa tin được
    VCAN_STATE_IDLE      = 1,
    VCAN_STATE_DISCHARGE = 2,
    VCAN_STATE_CHARGE    = 3,
    VCAN_STATE_FAULT     = 4    // relay đã ngắt
} VCAN_State_t;

// Bit 0-2 lấy NGUYÊN từ byte status của slave. Slave chỉ dùng 3 bit thấp
// (0x01 / 0x02 / 0x04) nên bit 3 trở lên an toàn để master tự dùng.
#define VCAN_FLAG_OVER_VOLT     0x01
#define VCAN_FLAG_UNDER_VOLT    0x02
#define VCAN_FLAG_OVER_TEMP     0x04
#define VCAN_FLAG_OVER_CURRENT  0x08
#define VCAN_FLAG_SLAVE_LOST    0x10
#define VCAN_FLAG_RELAY_OPEN    0x20
#define VCAN_FLAG_MANUAL_OFF    0x40    // người vận hành tự ngắt qua web - KHÁC
                                        // với ngắt do lỗi, xe phản ứng khác nhau
#define VCAN_FLAG_BMS_INTERNAL  0x80

#define VCAN_SOC_INVALID        0xFF
#define VCAN_TEMP_INVALID       0x80    // int8 = -128
#define VCAN_CURRENT_INVALID    (-32768)    // int16 min -> byte 2-3 = 0x80 0x00.
                                            // Cùng thủ pháp VCAN_TEMP_INVALID: khi
                                            // mất cảm biến dòng thì báo thẳng
                                            // "không biết", vì gửi 0 A lúc đó là
                                            // gửi một con số TRÔNG hợp lý.

class VehicleCAN_Manager {
private:
    MCP_CAN mcp;
    bool    isReady;
    bool    seenAll;        // đã từng thấy đủ mọi bình? phân biệt INIT với FAULT
    uint8_t aliveCounter;
    uint8_t muxIndex;       // bình nào sẽ gửi trong frame 0x201 lần tới
    uint8_t txFailRun;      // số lần TX thất bại LIÊN TIẾP

    void buildStatus(const BMS_Pack_State *snaps, uint8_t *out);
    void buildPackDetail(const BMS_Pack_State *snaps, uint8_t *out);
    void txFrame(uint32_t id, const uint8_t *data);

public:
    VehicleCAN_Manager(uint8_t csPin);
    bool init();
    void update();          // 1 snapshot -> đóng và gửi CẢ HAI frame
};

void Task_VehicleCAN_Run(void *pvParameters);

#endif
