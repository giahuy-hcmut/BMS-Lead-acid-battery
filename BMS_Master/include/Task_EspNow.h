/**
 * @file      Task_EspNow.h
 * @author    Do Dang Gia Huy
 * @brief     Telemetry broadcast to the handheld remote over ESP-NOW.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * The task is built but NOT started: its xTaskCreatePinnedToCore() call in
 * main.cpp is commented out. It is kept compiling on purpose, so the link to
 * the remote can be brought back without resurrecting dead code.
 *
 * ESP-NOW and the web access point share the single radio, so both must sit
 * on kWifiApChannel.
 */

#ifndef TASK_ESPNOW_H_
#define TASK_ESPNOW_H_

/**********************
 *      INCLUDES
 *********************/

#include <Arduino.h>

#include <esp_now.h>

#include "System_Data.h"

/**********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/** Owns the peer registration and the outgoing telemetry buffer. */
class EspNowManager
{
public:
    /**
     * @brief  Copy the target address and clear the telemetry buffer.
     * @param[in] mac  Six-byte MAC address of the remote.
     */
    explicit EspNowManager(const uint8_t *mac);

    /**
     * @brief  Start ESP-NOW, pin the channel and register the peer.
     * @return true once the peer is registered.
     */
    bool init();

    /**
     * @brief  Take a snapshot, pack it and send one telemetry frame.
     * @return None.
     */
    void sendTelemetry();

private:
    uint8_t targetMac_[6];

    /** Value-initialised, which matters: init() only fills peer_addr,
     * channel and encrypt, and esp_now_add_peer() reads the other fields
     * too - ifidx among them. Left indeterminate, the peer was registered
     * against whatever happened to be on the stack. */
    esp_now_peer_info_t peerInfo_{};

    BmsTelemetryPacket outgoingData_;
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  FreeRTOS entry point for the ESP-NOW broadcaster.
 * @param[in] pvParameters  Unused, required by the FreeRTOS signature.
 * @return None.
 */
void Task_EspNow_Run(void *pvParameters);

/**********************
 *      MACROS
 **********************/

#endif /*TASK_ESPNOW_H_*/
