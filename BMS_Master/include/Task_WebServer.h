#ifndef TASK_WEBSERVER_H
#define TASK_WEBSERVER_H

#include <Arduino.h>
#include "System_Data.h"
#include <ESPAsyncWebServer.h> // Thư viện ta vừa thêm

class WebServer_Manager {
private:
    AsyncWebServer* server;
    AsyncEventSource* events;
    
    // Hàm tạo chuỗi JSON (Chuyển dữ liệu C++ thành định dạng Web hiểu được)
    String buildJsonString();

public:
    WebServer_Manager();
    bool init();
    void broadcastData();
};

void Task_WebServer_Run(void *pvParameters);

#endif