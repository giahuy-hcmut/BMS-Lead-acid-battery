#include "Task_VehicleCAN.h"

VehicleCAN_Manager::VehicleCAN_Manager(uint8_t csPin) : mcp(csPin) {
    isReady      = false;
    seenAll      = false;
    aliveCounter = 0;
    muxIndex     = 0;
    txFailRun    = 0;
}

bool VehicleCAN_Manager::init() {
    /* MCP_8MHZ phải khớp thạch anh in trên board (X1 = "8.000" trên HW-184).
     * Khai MCP_16MHZ như phần lớn ví dụ trên mạng làm bit timing sai đúng gấp
     * đôi -> baudrate còn một nửa -> bus IM LẶNG HOÀN TOÀN, không lỗi nào báo.
     * Đây là lỗi phổ biến số 1 với MCP2515. Soi CNF1/2/3 bằng logic analyzer
     * trên SPI là thấy ngay. */
    if (mcp.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
        Serial.println("[VCAN] MCP2515 begin FAILED - kiem day SPI va nguon");
        isReady = false;
        return false;
    }

#ifdef VCAN_LOOPBACK
    mcp.setMode(MCP_LOOPBACK);      /* bring-up: khong can bus nao */
    Serial.println("[VCAN] MCP2515 ready - LOOPBACK");
#else
    mcp.setMode(MCP_NORMAL);
    Serial.println("[VCAN] MCP2515 ready - 500 kbps, 8 MHz xtal");
#endif

    isReady = true;
    return true;
}

void VehicleCAN_Manager::buildStatus(const BMS_Pack_State *snaps, uint8_t *out) {
    /* Tất cả suy từ snapshot mà task này đã có. Không thêm field nào vào
     * System_Data: đóng gói là việc của task, kho vẫn chỉ giữ dữ liệu. */
    float   totalV    = System_TotalVoltage(snaps);   /* đã bỏ bình offline */
    float   packI     = snaps[0].current;             /* dòng như nhau qua pack nối tiếp */
    int     soc       = System_MinSoc(snaps);
    uint8_t flags     = 0;
    bool    allOnline = true;

    for (uint8_t i = 0; i < System_GetPackCount(); i++) {
        if (snaps[i].isConnected) {
            flags |= snaps[i].status;   /* slave chỉ dùng bit 0-2 */
        } else {
            allOnline = false;          /* KHÔNG OR status bình offline: số cũ */
        }
    }

    /* INIT cho tới khi từng thấy đủ mọi bình. Sau đó mất kết nối là FAULT,
     * không phải "vẫn đang khởi động". */
    if (allOnline) { seenAll = true; }

    if (!allOnline)                    { flags |= VCAN_FLAG_SLAVE_LOST;   }
    if (packI > MAX_DISCHARGE_CURRENT) { flags |= VCAN_FLAG_OVER_CURRENT; }
    if (systemLocked)                  { flags |= VCAN_FLAG_RELAY_OPEN;   }
    if (webForceRelayOff)              { flags |= VCAN_FLAG_MANUAL_OFF;   }

    /* ACS758_ZERO_CURRENT là vùng chết mà Task_Current đã áp: dưới ngưỡng đó nó
     * ép dòng về đúng 0.0, nên dùng lại cùng con số làm ranh giới "đang nghỉ"
     * thay vì gõ một hằng số thứ hai có thể lệch. */
    const float idleBand = (float)ACS758_ZERO_CURRENT;

    VCAN_State_t state;
    if      (!seenAll)             { state = VCAN_STATE_INIT;      }
    else if (systemLocked)         { state = VCAN_STATE_FAULT;     }
    else if (packI >  idleBand)    { state = VCAN_STATE_DISCHARGE; }
    else if (packI < -idleBand)    { state = VCAN_STATE_CHARGE;    }
    else                           { state = VCAN_STATE_IDLE;      }

    /* Clamp TRƯỚC khi scale, cùng lý do như sendCurrentFrame: int16 chỉ tới
     * +/-327.67 A, cảm biến lỗi sẽ làm tràn và ĐẢO DẤU - xe sẽ tưởng pack đang
     * được sạc trong khi thực tế đang xả. */
    if (packI >  320.0f) { packI =  320.0f; }
    if (packI < -320.0f) { packI = -320.0f; }

    uint16_t vRaw = (uint16_t)lroundf(totalV * 100.0f);
    int16_t  iRaw = (int16_t) lroundf(packI  * 100.0f);

    out[0] = (uint8_t)((vRaw >> 8) & 0xFF);     /* MSB truoc */
    out[1] = (uint8_t)( vRaw       & 0xFF);
    out[2] = (uint8_t)((iRaw >> 8) & 0xFF);
    out[3] = (uint8_t)( iRaw       & 0xFF);
    out[4] = (soc < 0) ? VCAN_SOC_INVALID : (uint8_t)soc;
    out[5] = (uint8_t)state;
    out[6] = flags;
    out[7] = aliveCounter++;
}

void VehicleCAN_Manager::buildPackDetail(const BMS_Pack_State *snaps, uint8_t *out) {
    uint8_t count = System_GetPackCount();
    uint8_t idx   = muxIndex;

    /* Xoay chỉ số theo count chứ không theo TOTAL_PACKS: khi web cho nhập số
     * pack, vòng quét tự co lại thay vì gửi frame rỗng cho bình không tồn tại. */
    muxIndex = (uint8_t)((muxIndex + 1U) % count);

    bool     online = snaps[idx].isConnected;
    uint16_t vRaw   = (uint16_t)lroundf(snaps[idx].voltage * 100.0f);

    out[0] = idx;
    out[1] = online ? (uint8_t)((vRaw >> 8) & 0xFF) : 0x00;
    out[2] = online ? (uint8_t)( vRaw       & 0xFF) : 0x00;
    /* Bình offline: áp/SOC/nhiệt lưu lại đều là số CŨ -> báo không xác định,
     * cùng cách web hiện "--" cho chúng. */
    out[3] = online ? (uint8_t)snaps[idx].soc         : VCAN_SOC_INVALID;
    out[4] = online ? (uint8_t)snaps[idx].temperature : VCAN_TEMP_INVALID;
    out[5] = online ? snaps[idx].status               : 0x00;
    out[6] = online ? 1U : 0U;
    out[7] = count;
}

void VehicleCAN_Manager::txFrame(uint32_t id, const uint8_t *data) {
    if (mcp.sendMsgBuf(id, 0, 8, (uint8_t *)data) == CAN_OK) {
        txFailRun = 0;
        return;
    }

    /* MCP2515 KHÔNG tự thoát bus-off như bxCAN của STM32 (bên slave đã bật
     * AutoBusOff và phần cứng tự lo). Ở đây một chuỗi thất bại phải re-init bằng
     * tay, nếu không link lên xe chết cho tới khi tắt nguồn - âm thầm. */
    if (++txFailRun >= VCAN_TX_FAIL_LIMIT) {
        Serial.println("[VCAN] TX tac -> re-init MCP2515");
        txFailRun = 0;
        init();
    }
}

void VehicleCAN_Manager::update() {
    if (!isReady) { return; }

    /* MỘT lần lấy snapshot cho cả hai frame: System_Get_Snapshot() khoá mutex,
     * gọi hai lần mỗi chu kỳ là khoá hai lần không cần thiết. Và nhờ vậy hai
     * frame mô tả CÙNG một thời điểm. */
    BMS_Pack_State snaps[TOTAL_PACKS];
    System_Get_Snapshot(snaps);

    uint8_t data[8];

    buildStatus(snaps, data);
    txFrame(VCAN_STATUS_ID, data);

    buildPackDetail(snaps, data);
    txFrame(VCAN_PACK_ID, data);
}

// --- FREE RTOS WRAPPER ---
void Task_VehicleCAN_Run(void *pvParameters) {
    VehicleCAN_Manager vcan(PIN_VCAN_CS);

    if (!vcan.init()) {
        Serial.println("[VCAN] init failed -> delete task");
        vTaskDelete(NULL);
    }

    /* vTaskDelayUntil, KHÔNG phải vTaskDelay: cái sau đo chu kỳ từ lúc công việc
     * KẾT THÚC nên thời gian thực thi bị cộng vào mỗi vòng và nhịp trôi dần.
     * Đúng loại lỗi mà scheduler bên slave đã phải sửa (nạp lại lịch TRƯỚC khi
     * chạy thân task). */
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        vcan.update();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(VCAN_PERIOD_MS));
    }
}
