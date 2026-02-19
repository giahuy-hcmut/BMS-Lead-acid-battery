#include "Task_EspNow.h"
#include <WiFi.h>
#include <esp_wifi.h>

// [ĐÃ THAY ĐỔI: Chuyển các biến toàn cục (outgoingData, peerInfo) thành thành viên của Class EspNow_Manager]
EspNow_Manager::EspNow_Manager(const uint8_t* mac) {
    memcpy(targetMac, mac, 6);
    memset(&outgoingData, 0, sizeof(outgoingData));
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // In ra màn hình trạng thái gửi
    Serial.print("[ESP-NOW TX] Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success ✅" : "Delivery Fail ❌");
}

// 2. Sửa hàm init() để đăng ký hàm báo cáo trên:
bool EspNow_Manager::init() {
    WiFi.mode(WIFI_STA);

    // --- THÊM DÒNG NÀY: Ép chạy Kênh 1 ---
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) return false;

    // --- THÊM DÒNG NÀY ---
    esp_now_register_send_cb(OnDataSent); 

    memcpy(peerInfo.peer_addr, targetMac, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) return false;
    return true;
}

void EspNow_Manager::sendTelemetry() {
    // Lấy dữ liệu mới nhất từ kho System_Data
    // [ĐÃ THAY ĐỔI: Đổi số 5 thành TOTAL_PACKS để tránh lỗi khi thay số lượng bình]
    BMS_Pack_State snapPacks[TOTAL_PACKS];
    System_Get_Snapshot(snapPacks);

    // Đóng gói
    outgoingData.totalVoltage = 0;
    outgoingData.systemCurrent = snapPacks[0].current; // Giả sử dòng điện tổng nằm ở pack 0

    for (int i = 0; i < TOTAL_PACKS; i++) {
        // [ĐÃ SỬA LỖI] THÊM DÒNG NÀY ĐỂ TÍNH TRẠNG THÁI ONLINE
        bool isOnline = (snapPacks[i].lastUpdate > 0) && (millis() - snapPacks[i].lastUpdate < LCD_TIMEOUT);
        outgoingData.packVolts[i] = snapPacks[i].voltage;
        outgoingData.isOnline[i] = snapPacks[i].isConnected;
        
        // Chỉ cộng dồn áp tổng nếu bình đó đang Online
        if (snapPacks[i].isConnected) {
            outgoingData.totalVoltage += snapPacks[i].voltage;
        }
    }

    // Bắn dữ liệu đi
    esp_now_send(targetMac, (uint8_t *) &outgoingData, sizeof(outgoingData));
}

// --- FREE RTOS WRAPPER ---
// [ĐÃ THAY ĐỔI: Viết lại hàm Wrapper để bọc toàn bộ Class OOP lại]
void Task_EspNow_Run(void *pvParameters) {
    // Kéo địa chỉ MAC từ Config.h vào
    const uint8_t mac[] = REMOTE_MAC_ADDRESS;
    EspNow_Manager myEspNow(mac);

    if (!myEspNow.init()) {
        vTaskDelete(NULL);
    }

    // 4. Vòng lặp bắn dữ liệu liên tục
    while (1) {
        myEspNow.sendTelemetry();
        
        // Nghỉ 500ms rồi bắn tiếp (Đảm bảo LCD trên tay cầm không bị nháy)
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}