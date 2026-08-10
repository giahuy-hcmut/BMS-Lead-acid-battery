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
    /* Non-blocking poll. A 10 ms blocking receive made this loop turn every
     * ~11 ms - the slaves only send 5 frames per second, so the wait almost
     * always ran to full timeout. A 5 ms master frame is then impossible, and
     * it would have failed silently: the interval macro would say 5 while the
     * bus showed 11. */
    if (twai_receive(&rx_msg, pdMS_TO_TICKS(CAN_RX_POLL_TIMEOUT_MS)) == ESP_OK) {
        
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


// --- FREE RTOS WRAPPER ---
void Task_CAN_Run(void *pvParameters) {
    // [ĐÃ THAY ĐỔI: Sử dụng chân cấu hình từ Macro trong Config.h]
    CAN_Manager myCanBus(PIN_CAN_TX, PIN_CAN_RX, CAN_BAUD_RATE);

    if (!myCanBus.init()) {
        Serial.println("CAN Init Failed -> Delete Task");
        vTaskDelete(NULL);
    }

    BMS_Message_t tempMsg;
    TickType_t lastFrame = xTaskGetTickCount();

    while (1) {
        if (myCanBus.readMessage(tempMsg)) {
            xQueueSend(canQueue, &tempMsg, 0);
        }
        /* webSlavesActive stays: clearing it from the web UI is still the way
         * to let the slaves fall asleep, since 0x100 doubles as heartbeat. */
        if ((xTaskGetTickCount() - lastFrame) >= pdMS_TO_TICKS(CAN_MASTER_INTERVAL_MS)) {
            if (webSlavesActive) myCanBus.sendCurrentFrame(System_Get_Current());
            lastFrame = xTaskGetTickCount();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}