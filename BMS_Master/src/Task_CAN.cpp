#include "Task_CAN.h"

// --- IMPLEMENTATION CỦA CLASS CAN_MANAGER ---

CAN_Manager::CAN_Manager(int tx, int rx, long baud) {
    txPin = (gpio_num_t)tx;
    rxPin = (gpio_num_t)rx;
    baudRate = baud;
    isReady = false;
}

bool CAN_Manager::init() {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(txPin, rxPin, TWAI_MODE_NORMAL);
    
    // Cấu hình tốc độ
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    if (baudRate == 250000) t_config = TWAI_TIMING_CONFIG_250KBITS();
    else if (baudRate == 1000000) t_config = TWAI_TIMING_CONFIG_1MBITS();
    
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        if (twai_start() == ESP_OK) {
            isReady = true;
            // Chỉ in 1 dòng báo khởi động thành công, không spam
            Serial.println("[CAN] Driver Started Successfully");
            return true;
        }
    }
    
    Serial.println("[CAN] Driver Failed!");
    return false;
}

void CAN_Manager::sendCurrentFrame(float current_a) {
    if (!isReady) return;

    twai_status_info_t status;
    twai_get_status_info(&status);

    if (status.state == TWAI_STATE_BUS_OFF) {
        twai_initiate_recovery();
        return;
    }
    if (status.state == TWAI_STATE_STOPPED) {
        twai_start();
        return;
    }
    if (status.state == TWAI_STATE_RECOVERING) {
        return;
    }

    /* Clamp before scaling: int16 spans +/-327.67 A. A wild sensor reading
     * would otherwise wrap sign and tell every slave the pack is charging
     * while it is really being discharged - the SOC would climb under load
     * with no fault raised anywhere. */
    if (current_a >  320.0f) { current_a =  320.0f; }
    if (current_a < -320.0f) { current_a = -320.0f; }

    int16_t raw = (int16_t)lroundf(current_a * 100.0f);

    twai_message_t msg = {};
    msg.identifier = CAN_MASTER_ID;
    msg.data_length_code = 2;                        /* was 0 - empty heartbeat */
    msg.data[0] = (uint8_t)((raw >> 8) & 0xFF);      /* MSB first, matching   */
    msg.data[1] = (uint8_t)(raw & 0xFF);             /* bytes 2-3 of a slave  */
    twai_transmit(&msg, pdMS_TO_TICKS(CAN_TX_TIMEOUT_MS));
}

bool CAN_Manager::readMessage(BMS_Message_t &msgOut) {
    if (!isReady) return false;

    twai_message_t rx_msg;
    /* CHAN vo han. Truoc day day la tham do (timeout 0) vi CUNG task nay con phai
     * gui 0x100 moi 5 ms - chan o day thi khong bao gio toi luot gui. Viec gui da
     * tach sang Task_CAN_Tx_Run nen gio chan duoc.
     *
     * Ben trong, twai_receive() la xQueueReceive tren hang doi ma ISR cua driver
     * nap vao. Nen: khong frame -> task Blocked, 0% CPU; co frame -> ISR danh thuc,
     * va vi task nay prio 5 (cao nhat loi 1) nen portYIELD_FROM_ISR chuyen ngu canh
     * ngay cuoi ISR. Tre ~us thay vi <=1 ms cua vong tham do cu. */
    if (twai_receive(&rx_msg, portMAX_DELAY) == ESP_OK) {
        
        msgOut.can_id = rx_msg.identifier;
        
        // Giải mã Voltage
        if (rx_msg.data_length_code >= 2) {
            uint16_t raw_vol = (rx_msg.data[0] << 8) | rx_msg.data[1];
            msgOut.voltage = raw_vol / 100.0f;
        } else {
            msgOut.voltage = 0.0f;
        }

        /* Byte 4 = SOC (%) from the slave's Kalman filter.
         *
         * Bytes 2-3 carry the current and are deliberately NOT decoded: that is
         * the master's OWN value coming back from frame 0x100, so reading it
         * would create a second source of truth for something already known.
         * Only worth decoding later as a loopback diagnostic. */
        if (rx_msg.data_length_code >= 5) {
            msgOut.soc = rx_msg.data[4];
        } else {
            msgOut.soc = 0;
        }

        // BỔ SUNG: Giải mã Nhiệt độ (trừ đi 40 offset) và Mã lỗi
        if (rx_msg.data_length_code >= 7) {
            msgOut.temperature = (int8_t)rx_msg.data[5] - 40;
            msgOut.status = rx_msg.data[6];
        } else {
            msgOut.temperature = 0;
            msgOut.status = 0;
        }
        
        return true; 
    }
    return false; 
}


/* MOT doi tuong, HAI task dung chung. Driver TWAI cua ESP-IDF la singleton -
 * twai_driver_install() chi duoc goi MOT lan - nen doi tuong phai o file scope
 * thay vi tao trong tung task.
 *
 * Khong can bat tay dong bo nao: Rx goi init(), con sendCurrentFrame() da co san
 * `if (!isReady) return;` ngay dong dau, nen Tx tu im lang cho toi khi Rx init
 * xong.
 *
 * Constructor chi gan 4 bien, khong dung phan cung, nen an toan khi chay truoc
 * setup() nhu moi bien toan cuc C++ khac. */
static CAN_Manager s_canBus(PIN_CAN_TX, PIN_CAN_RX, CAN_BAUD_RATE);

// --- RX: SU KIEN. Ngu 0% CPU khi bus im, thuc ~us sau khi co ngat. ---
void Task_CAN_Rx_Run(void *pvParameters) {
    if (!s_canBus.init()) {
        Serial.println("[CAN] Init Failed -> Delete Rx Task");
        vTaskDelete(NULL);
    }

    BMS_Message_t tempMsg;

    for (;;) {
        if (s_canBus.readMessage(tempMsg)) {    /* CHAN o day */
            xQueueSend(canQueue, &tempMsg, 0);
        } else {
            /* portMAX_DELAY khong bao gio timeout, nen false = LOI DRIVER: dang
             * recover sau bus-off, hoac driver stopped. Khong co delay nay thi
             * vong lap quay 100% CPU suot thoi gian bus con hong.
             *
             * Tx moi la ben goi twai_initiate_recovery() (loi bus-off phat hien
             * tu phia gui), nen hai task tu phoi hop: Tx sua bus, Rx doi. */
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

// --- TX: CHU KY. 0x100 cho dong pack, kiem luon vai heartbeat cho slave. ---
void Task_CAN_Tx_Run(void *pvParameters) {
    /* vTaskDelayUntil, KHONG phai vTaskDelay: cai sau do chu ky tu luc cong viec
     * KET THUC nen thoi gian thuc thi bi cong vao moi vong va nhip troi dan. Cach
     * cu (vTaskDelay(1ms) + so tick) con lam nhip bi luong tu hoa theo tick 1 ms,
     * thinh thoang ra 6 ms thay vi 5. Cung ly do Task_VehicleCAN dung DelayUntil. */
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        /* System_Get_SlavesActive() giu nguyen: tat no tu web van la cach cho slave
         * ngu, vi 0x100 kiem luon vai heartbeat. */
        if (System_Get_SlavesActive()) {
            s_canBus.sendCurrentFrame(System_Get_Current());
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(CAN_MASTER_INTERVAL_MS));
    }
}