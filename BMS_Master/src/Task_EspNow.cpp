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
    WiFi.mode(WIFI_AP_STA);

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
    BMS_Pack_State snapPacks[TOTAL_PACKS];
    System_Get_Snapshot(snapPacks);

    outgoingData.totalVoltage = 0;
    outgoingData.systemCurrent = snapPacks[0].current; 
    outgoingData.systemSOC = snapPacks[0].soc; // <--- THÊM SOC

    for (int i = 0; i < TOTAL_PACKS; i++) {
        outgoingData.packVolts[i] = snapPacks[i].voltage;
        outgoingData.packTemps[i] = snapPacks[i].temperature; // <--- THÊM NHIỆT ĐỘ
        outgoingData.packStatus[i] = snapPacks[i].status;     // <--- THÊM MÃ LỖI
        outgoingData.isOnline[i] = snapPacks[i].isConnected;
        
        if (snapPacks[i].isConnected) {
            outgoingData.totalVoltage += snapPacks[i].voltage;
        }
    }

    Serial.printf("[ESP-NOW] Packing Data... Total Volt: %.2f V | SOC: %d %%\n", outgoingData.totalVoltage, outgoingData.systemSOC);
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