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
    BMS_Pack_State snapPacks[kTotalPacks];
    System_Get_Snapshot(snapPacks);

    outgoingData.totalVoltage  = System_TotalVoltage(snapPacks);
    outgoingData.systemCurrent = snapPacks[0].current;
    // SOC he thong = min cua cac binh; -1 khi co slave offline.
    // Tay cam se nhan -1 -> can hien "--" o firmware ben do.
    outgoingData.systemSOC = System_MinSoc(snapPacks);

    for (int i = 0; i < kTotalPacks; i++) {
        outgoingData.packVolts[i] = snapPacks[i].voltage;
        outgoingData.packTemps[i] = snapPacks[i].temperature; // <--- THÊM NHIỆT ĐỘ
        outgoingData.packStatus[i] = snapPacks[i].status;     // <--- THÊM MÃ LỖI
        outgoingData.isOnline[i] = snapPacks[i].isConnected;
    }

    Serial.printf("[ESP-NOW] Packing Data... Total Volt: %.2f V | SOC: %d %%\n", outgoingData.totalVoltage, outgoingData.systemSOC);
    esp_now_send(targetMac, (uint8_t *) &outgoingData, sizeof(outgoingData));
}

// --- FREE RTOS WRAPPER ---
// [ĐÃ THAY ĐỔI: Viết lại hàm Wrapper để bọc toàn bộ Class OOP lại]
void Task_EspNow_Run(void *pvParameters) {
    // Kéo địa chỉ MAC từ Config.h vào
    /* kRemoteMac is defined in Config.h. */
    EspNow_Manager myEspNow(kRemoteMac);

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