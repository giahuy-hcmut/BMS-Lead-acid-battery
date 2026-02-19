#include "Task_EspNowRx.h"
#include <WiFi.h>
#include <esp_wifi.h> // Thêm thư viện này ở đầu file

// Hàm Callback thực thi khi sóng bay tới (Chạy trong luồng ngắt của WiFi)
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len == sizeof(BMS_Telemetry_Packet)) {
        BMS_Telemetry_Packet packet;
        memcpy(&packet, incomingData, sizeof(packet));
        // Đẩy vào hàng đợi cho Task chính xử lý
        xQueueSendFromISR(espNowQueue, &packet, NULL);
    }
}

EspNowRx_Manager::EspNowRx_Manager() {}

bool EspNowRx_Manager::init() {
    WiFi.mode(WIFI_STA);

    // --- THÊM DÒNG NÀY: Ép chạy Kênh 1 ---
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Init Failed");
        return false;
    }
    esp_now_register_recv_cb(OnDataRecv);
    Serial.println("[ESP-NOW] Receiver Ready!");
    return true;
}

void EspNowRx_Manager::processQueue() {
    BMS_Telemetry_Packet packet;
    // Chờ lấy dữ liệu từ Queue (Block vô hạn nếu không có sóng)
    if (xQueueReceive(espNowQueue, &packet, portMAX_DELAY) == pdTRUE) {
        System_Update_Telemetry(&packet);
    }
}

void Task_EspNowRx_Run(void *pvParameters) {
    EspNowRx_Manager myReceiver;
    if (!myReceiver.init()) vTaskDelete(NULL);

    while (1) {
        myReceiver.processQueue();
    }
}