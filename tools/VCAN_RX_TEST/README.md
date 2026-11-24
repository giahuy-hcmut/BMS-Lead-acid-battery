# VCAN_RX_TEST — BMS → vehicle CAN receiver

Bộ node "xe" giả lập để **nhận + giải mã** gói CAN mà BMS master phát lên bus xe
(`0x200` STATUS, `0x201` PACK). Dùng cho 2 việc:

1. **Test**: xác nhận master thực sự đẩy được frame ra CAN_H/CAN_L (và tạo ACK
   để master hết in `[VCAN] TX tac`).
2. **Bàn giao**: đưa file `include/BMS_VehicleCAN.h` cho người điều khiển motor —
   họ chỉ cần include + gọi 2 hàm decode, **không phải tự giải mã byte**.

## Phần cứng (ESP32 thứ 2 + MCP2551)

| ESP32 | ↔ | MCP2551 |
|---|---|---|
| GPIO5 (TX) | → | TXD (pin 1) |
| GPIO4 (RX) | ← | RXD (pin 4) |
| 5V / GND | → | VCC / GND |
| — | | Rs (pin 8) → GND (chế độ high-speed) |

- `CANH`/`CANL` của MCP2551 nối vào bus xe (CANH/CANL của MCP2515 bên master).
- **120 Ω** ngang CANH-CANL ở **mỗi đầu** bus (tổng 2 cái).
- **GND CHUNG** với master. Baud **500 kbps**.

## Chạy test

```
pio run -t upload       # nạp con ESP32 nhận
pio device monitor      # xem Serial 115200
```

Kết quả mong đợi (đối chiếu với web dashboard của master):

```
ID 0x200 DLC 8: 17 70 00 00 4B 02 00 1F
  STATUS  Vtong=60.00V  I=+0.00A  SOC=75  state=DISCHARGE  cnt=31  flags=[none]
ID 0x201 DLC 8: 00 04 A2 4B 19 00 01 05
  PACK[0] V=11.86V  SOC=75  T=25  online=1  (n=5)  flags=[none]
```

- **`cnt` phải TĂNG** mỗi frame 0x200. Đứng yên ⇒ BMS treo (timeout frame không bắt được).
- Không thấy frame ⇒ kiểm dây CANH/CANL, 120 Ω × 2, GND chung, thạch anh MCP2515 master (8 MHz).

## Tích hợp vào project motor (dành cho đồng đội)

Chỉ cần **1 file**: copy `include/BMS_VehicleCAN.h` vào project của bạn. Nó
**không đụng phần cứng CAN** — bạn giữ nguyên vòng nhận CAN sẵn có (TWAI, hoặc
MCP2515/mcp_can…), chỉ móc `(id, data, len)` vào hàm decode:

```cpp
#include "BMS_VehicleCAN.h"

// trong vòng nhận CAN của bạn, với mỗi frame thu được:
BmsStatus st;
if (BmsVcan_DecodeStatus(rxId, rxData, rxLen, &st)) {
    if (st.state == BMS_STATE_FAULT)      motor_stop();
    if (st.flags & BMS_FLAG_OVER_CURRENT) motor_derate();
    // st.total_voltage_v, st.current_a, st.current_valid, st.sys_soc ...
}

BmsPack pk;
if (BmsVcan_DecodePack(rxId, rxData, rxLen, &pk)) {
    // pk.index, pk.voltage_v, pk.soc, pk.temp_c, pk.online, pk.pack_count
}
```

Hai hàm trả `false` nếu id/độ dài không khớp ⇒ gọi vô tư trên mọi frame.

### ‼️ File này KHÔNG chạy một mình
`BMS_VehicleCAN.h` **chỉ GIẢI MÃ** (byte → struct). Nó **KHÔNG** khởi tạo CAN,
**KHÔNG** nhận frame. Bạn PHẢI tự cung cấp:
1. **Khởi tạo CAN** của bạn (`twai_driver_install`+`twai_start`, hoặc `mcp_can begin`…).
2. **Vòng nhận frame** (`twai_receive` / `readMsgBuf`) → lấy `(id, data, len)`.
3. Gọi 2 hàm decode ở trên → dùng struct.

`src/main.cpp` trong repo này là ví dụ ĐẦY ĐỦ cả 3 bước (để test). Bạn chỉ lấy
`.h`, tự làm bước 1-2 (bạn đã có sẵn cho motor).

### Setup tối thiểu (checklist)
- [ ] Copy `include/BMS_VehicleCAN.h` vào project.
- [ ] CAN chạy **500 kbps**, nhận được **0x200** + **0x201** (filter accept-all hoặc nhận 2 ID này).
- [ ] Khai `BmsStatus st;` và/hoặc `BmsPack pk;`.
- [ ] Mỗi frame: gọi `BmsVcan_DecodeStatus/DecodePack(id, data, len, &...)`.
- [ ] Kiểm hàm trả `true` trước khi dùng struct.

### API → biến → thông số
| Gọi hàm | Đổ vào | Lấy thông số |
|---|---|---|
| `BmsVcan_DecodeStatus(id,data,len,&st)` | `BmsStatus st` | `st.total_voltage_v` V · `st.current_a` A(>0 xả) · `st.current_valid` · `st.sys_soc` %(−1=??) · `st.state` (`BMS_STATE_*`) · `st.flags` (`BMS_FLAG_*`) · `st.alive_counter` |
| `BmsVcan_DecodePack(id,data,len,&pk)` | `BmsPack pk` | `pk.index` · `pk.voltage_v` V · `pk.soc` %(−1=??) · `pk.temp_c` °C(−1000=??) · `pk.online` · `pk.flags` · `pk.pack_count` |
| `BmsVcan_StateName(st.state)` | — | chuỗi "FAULT"/"DISCHARGE"… để in log |

### Ví dụ dùng với FreeRTOS
Task nhận+giải mã ghi vào kho dùng chung (mutex), task motor đọc:

```cpp
#include "BMS_VehicleCAN.h"

static BmsStatus         g_bms;             // kho dùng chung
static SemaphoreHandle_t g_lock;

// TASK 1: nhận CAN + giải mã
void Task_BmsRx(void *pv) {
    twai_message_t m;                       // (TWAI đã install+start 500k ở nơi khác)
    for (;;) {
        if (twai_receive(&m, portMAX_DELAY) == ESP_OK) {
            BmsStatus st;
            if (BmsVcan_DecodeStatus(m.identifier, m.data, m.data_length_code, &st)) {
                xSemaphoreTake(g_lock, portMAX_DELAY);
                g_bms = st;                 // cất vào kho
                xSemaphoreGive(g_lock);
            }
            // 0x201 xử tương tự nếu cần từng bình
        }
    }
}

// TASK 2: điều khiển motor - đọc kho
void Task_Motor(void *pv) {
    for (;;) {
        BmsStatus s;
        xSemaphoreTake(g_lock, portMAX_DELAY);
        s = g_bms;                          // copy ra
        xSemaphoreGive(g_lock);

        if      (s.state == BMS_STATE_FAULT)      motor_stop();
        else if (s.flags & BMS_FLAG_OVER_CURRENT) motor_derate();
        else                                      motor_run(s.sys_soc);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup_bms() { g_lock = xSemaphoreCreateMutex(); }
```

Nguyên tắc: **1 task nhận+giải** ghi kho (mutex), **task motor** đọc kho — mẫu
producer/consumer như BMS master. Nếu **1 task lo cả CAN lẫn motor** thì khỏi
mutex: decode xong dùng luôn.

## Giao thức (cố định bởi firmware master, MSB trước)

**`0x200` BMS_STATUS** — DLC 8, mỗi 100 ms
| byte | nội dung | kiểu | đơn vị | vô hiệu |
|---|---|---|---|---|
| 0-1 | tổng áp | uint16 | 0.01 V | |
| 2-3 | dòng pack | int16 | 0.01 A (>0 xả) | `-32768` = mất cảm biến |
| 4 | SOC hệ thống | uint8 | 1 % | `0xFF` |
| 5 | trạng thái | uint8 | 0 INIT·1 IDLE·2 XẢ·3 SẠC·4 FAULT | |
| 6 | cờ lỗi | uint8 | bitmask | |
| 7 | bộ đếm sống | uint8 | +1 mỗi frame | |

**`0x201` BMS_PACK** — DLC 8, mỗi 100 ms, mỗi frame 1 bình (xoay vòng)
| byte | nội dung | kiểu | đơn vị | vô hiệu |
|---|---|---|---|---|
| 0 | chỉ số bình | uint8 | 0..count-1 | |
| 1-2 | áp bình | uint16 | 0.01 V | |
| 3 | SOC bình | uint8 | 1 % | `0xFF` |
| 4 | nhiệt bình | int8 | 1 °C | `0x80` |
| 5 | cờ lỗi bình | uint8 | | |
| 6 | online | uint8 | 0/1 | |
| 7 | số bình giám sát | uint8 | | |

**Cờ lỗi** (byte 6 của 0x200): `01` quá áp · `02` sụt áp · `04` quá nhiệt ·
`08` quá dòng · `10` mất slave · `20` relay ngắt · `40` người tắt (web, KHÁC lỗi)
· `80` lỗi nội bộ BMS.
