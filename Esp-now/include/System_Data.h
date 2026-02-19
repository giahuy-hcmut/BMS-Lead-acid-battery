#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include "Config.h"

// 1. Cấu trúc gói tin (BẮT BUỘC PHẢI Y HỆT BÊN MASTER)
typedef struct {
    float totalVoltage;
    float systemCurrent;
    float packVolts[TOTAL_PACKS];
    bool  isOnline[TOTAL_PACKS];
} BMS_Telemetry_Packet;

// 2. Cấu trúc trạng thái hệ thống cục bộ (Lưu thêm thời gian nhận)
struct Remote_System_State {
    BMS_Telemetry_Packet telemetry;
    uint32_t lastRecvTime;
    bool isConnected;
};

// Biến toàn cục (extern)
extern Remote_System_State globalSystemState;
extern SemaphoreHandle_t dataMutex;
extern QueueHandle_t espNowQueue;

// API Hệ thống
void System_Data_Init();
void System_Update_Telemetry(const BMS_Telemetry_Packet* incomingData);
void System_Get_Snapshot(Remote_System_State* snapshot);

#endif