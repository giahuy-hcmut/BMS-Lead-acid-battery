/**
 * @file      Task_EspNow.cpp
 * @author    Do Dang Gia Huy
 * @brief     Peer setup and telemetry packing for the ESP-NOW link.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_EspNow.h"

#include <WiFi.h>
#include <esp_wifi.h>

#include <string.h>

/**********************
 *      DEFINES
 **********************/

/** Send on whichever interface the peer is reachable through. */
constexpr uint8_t kPeerDefaultChannel = 0U;

/** Telemetry period. Slow enough that the LCD on the remote does not
 * flicker. */
constexpr TickType_t kTelemetryPeriod = pdMS_TO_TICKS(500);

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

namespace
{
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
}

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Copy the target address and clear the telemetry buffer.
 * @param[in] mac  Six-byte MAC address of the remote.
 */
EspNowManager::EspNowManager(const uint8_t *mac)
{
    memcpy(targetMac_, mac, sizeof(targetMac_));
    memset(&outgoingData_, 0, sizeof(outgoingData_));
}

/**
 * @brief  Start ESP-NOW, pin the channel and register the peer.
 * @return true once the peer is registered.
 */
bool EspNowManager::init()
{
    WiFi.mode(WIFI_AP_STA);

    /* Force the radio onto the same channel the access point uses: ESP-NOW
     * and the AP share one radio, and a mismatch silently kills ESP-NOW. */
    esp_wifi_set_channel(kWifiApChannel, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK)
    {
        return false;
    }

    esp_now_register_send_cb(onDataSent);

    memcpy(peerInfo_.peer_addr, targetMac_, sizeof(targetMac_));
    peerInfo_.channel = kPeerDefaultChannel;
    peerInfo_.encrypt = false;

    if (esp_now_add_peer(&peerInfo_) != ESP_OK)
    {
        return false;
    }

    return true;
}

/**
 * @brief  Take a snapshot, pack it and send one telemetry frame.
 * @return None.
 */
void EspNowManager::sendTelemetry()
{
    BmsPackState snapPacks[kTotalPacks];

    System_Get_Snapshot(snapPacks);

    outgoingData_.totalVoltage  = System_TotalVoltage(snapPacks);
    outgoingData_.systemCurrent = snapPacks[0].current;

    /* System SOC is the minimum of the packs, and -1 when a slave is
     * offline. The remote receives that -1 and has to render "--" in its own
     * firmware. */
    outgoingData_.systemSOC = System_MinSoc(snapPacks);

    for (uint8_t i = 0U; i < kTotalPacks; i++)
    {
        outgoingData_.packVolts[i]  = snapPacks[i].voltage;
        outgoingData_.packTemps[i]  = snapPacks[i].temperature;
        outgoingData_.packStatus[i] = snapPacks[i].status;
        outgoingData_.isOnline[i]   = snapPacks[i].isConnected;
    }

    Serial.printf("[ESP-NOW] Packing data, total %.2f V, SOC %d %%\n",
                  outgoingData_.totalVoltage, outgoingData_.systemSOC);

    esp_now_send(targetMac_, (uint8_t *)&outgoingData_, sizeof(outgoingData_));
}

/**
 * @brief  FreeRTOS entry point for the ESP-NOW broadcaster.
 * @param[in] pvParameters  Unused.
 * @return None.
 */
void Task_EspNow_Run(void *pvParameters)
{
    EspNowManager myEspNow(kRemoteMac);

    (void)pvParameters;

    if (!myEspNow.init())
    {
        vTaskDelete(NULL);
    }

    while (1)
    {
        myEspNow.sendTelemetry();
        vTaskDelay(kTelemetryPeriod);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

namespace
{

/**
 * @brief  Transmission-complete callback registered with ESP-NOW.
 * @param[in] mac_addr  Peer the frame was addressed to; unused.
 * @param[in] status    Delivery result reported by the stack.
 * @return None.
 */
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    (void)mac_addr;

    Serial.print("[ESP-NOW TX] Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "delivery success"
                                                  : "delivery fail");
}

}  /* namespace */
