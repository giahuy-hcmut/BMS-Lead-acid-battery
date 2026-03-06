#include "Task_WebServer.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include "Web_HTML.h"

WebServer_Manager::WebServer_Manager() {
    server = new AsyncWebServer(80);      // Chạy server ở Port 80 (chuẩn Web)
    events = new AsyncEventSource("/events"); // Kênh truyền Real-time
}

bool WebServer_Manager::init() {
    // 1. Chuyển WiFi sang chế độ vừa làm AP (phát) vừa làm STA (để ESP-NOW chạy)
    WiFi.mode(WIFI_AP_STA);
    
    // 2. Cấu hình mạng phát ra (SSID, Pass, Channel, Ẩn=0, Số lượng truy cập=4)
    // CỰC KỲ QUAN TRỌNG: Kênh phải là 1 để khớp với ESP-NOW
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, 4);
    
    Serial.print("[WEB] Access Point Started! IP: ");
    Serial.println(WiFi.softAPIP());

    // 3. Cấu hình đường dẫn Web
    // Khi người dùng vào trang chủ "/", gửi file HTML cho họ
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });

    // Kích hoạt kênh sự kiện Real-time
    server->addHandler(events);

    // 4. Bắt đầu chạy Server
    server->begin();
    return true;
}

String WebServer_Manager::buildJsonString() {
    BMS_Pack_State snapPacks[TOTAL_PACKS];
    System_Get_Snapshot(snapPacks);

    float totalV = 0;
    float sysI = snapPacks[0].current;
    int sysSOC = snapPacks[0].soc; 

    String json = "{";
    
    json += "\"packs\":[";
    for(int i=0; i<TOTAL_PACKS; i++) {
        if (snapPacks[i].isConnected) totalV += snapPacks[i].voltage;
        
        json += "{\"volt\":";
        json += String(snapPacks[i].voltage, 2);
        
        // --- ĐÃ SỬA: Bổ sung temp và err cho JS đọc ---
        json += ",\"temp\":";
        json += String(snapPacks[i].temperature);
        json += ",\"err\":";
        json += String(snapPacks[i].status);
        // ----------------------------------------------

        json += ",\"online\":";
        json += snapPacks[i].isConnected ? "true" : "false";
        json += "}";
        
        if (i < TOTAL_PACKS - 1) json += ","; 
    }
    json += "],";
    
    json += "\"totalV\":"; json += String(totalV, 2); json += ",";
    
    // --- ĐÃ SỬA: Thay dấu '}' thành ',' để tiếp tục nối chuỗi ---
    json += "\"sysI\":"; json += String(sysI, 2); json += ",";
    
    // BỔ SUNG: % Pin tổng
    json += "\"soc\":"; json += String(sysSOC);
    
    // Đóng object JSON ở dòng cuối cùng
    json += "}";

    return json;
}

void WebServer_Manager::broadcastData() {
    // Chỉ gửi dữ liệu nếu có người đang mở trang Web
    if (events->count() > 0) {
        String jsonStr = buildJsonString();
        events->send(jsonStr.c_str(), "message", millis());
    }
}

// --- FREE RTOS WRAPPER ---
void Task_WebServer_Run(void *pvParameters) {
    WebServer_Manager myWeb;
    
    if (!myWeb.init()) vTaskDelete(NULL);

    while (1) {
        myWeb.broadcastData();
        // Cập nhật mỗi 500ms
        vTaskDelay(pdMS_TO_TICKS(WEB_UPDATE_INTERVAL)); 
    }
}