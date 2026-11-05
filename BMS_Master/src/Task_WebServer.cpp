/**
 * @file      Task_WebServer.cpp
 * @author    Do Dang Gia Huy
 * @brief     Access point setup, HTTP routes and JSON serialisation.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_WebServer.h"

#include <WiFi.h>
#include <esp_wifi.h>

#include "Web_HTML.h"

/**********************
 *      DEFINES
 **********************/

constexpr uint16_t kHttpPort      = 80U;
constexpr uint8_t  kApHidden      = 0U;     /**< broadcast the SSID */
constexpr uint8_t  kApMaxClients  = 4U;

/** Decimal places used for the voltage and current fields. */
constexpr int kJsonDecimals = 2;

/** Reserved up front so the dozens of appends below do not reallocate the
 * string on the heap every period. Five packs produce roughly 300 bytes. */
constexpr size_t kJsonReserve = 512U;

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

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
 * @brief  Construct the server on port 80 and the event stream.
 */
WebServerManager::WebServerManager()
    : server_(kHttpPort),
      events_("/events")
{
}

/**
 * @brief  Bring up the access point and register the routes.
 * @return true once the server is listening.
 */
bool WebServerManager::init()
{
    /* AP plus STA: the access point serves the dashboard while the station
     * interface is what ESP-NOW needs to exist at all. */
    WiFi.mode(WIFI_AP_STA);

    /* The channel MUST match the one ESP-NOW uses, or ESP-NOW stops
     * working: both share the single radio.
     *
     * The return value is checked. It used to be discarded, and this
     * function then returned true unconditionally, which made the caller's
     * `if (!init())` branch unreachable - a failure handler that could never
     * run. */
    if (!WiFi.softAP(kWifiApSsid, kWifiApPass, kWifiApChannel, kApHidden,
                     kApMaxClients))
    {
        Serial.println("[WEB] softAP failed - no access point");
        return false;
    }

    Serial.print("[WEB] Access point started, IP: ");
    Serial.println(WiFi.softAPIP());

    server_.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", index_html);
    });

    server_.addHandler(&events_);

    server_.on("/relay", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("state", true))
        {
            System_Set_RelayOverride(
                request->getParam("state", true)->value() == "off");
        }
        request->send(200, "text/plain", "OK");
    });

    server_.on("/slaves", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("state", true))
        {
            System_Set_SlavesActive(
                request->getParam("state", true)->value() == "on");
        }
        request->send(200, "text/plain", "OK");
    });

    server_.begin();
    return true;
}

/**
 * @brief  Push one JSON snapshot to every connected browser.
 * @return None.
 */
void WebServerManager::broadcastData()
{
    /* Nobody watching means nothing to serialise. */
    if (events_.count() > 0)
    {
        String jsonStr = buildJsonString();
        events_.send(jsonStr.c_str(), "message", millis());
    }
}

/**
 * @brief  FreeRTOS entry point for the web server.
 * @param[in] pvParameters  Unused.
 * @return None.
 */
void Task_WebServer_Run(void *pvParameters)
{
    WebServerManager myWeb;

    (void)pvParameters;

    if (!myWeb.init())
    {
        vTaskDelete(NULL);
    }

    while (1)
    {
        myWeb.broadcastData();
        vTaskDelay(pdMS_TO_TICKS(kWebUpdateIntervalMs));
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  Serialise the whole system state as JSON.
 * @return The JSON document.
 */
String WebServerManager::buildJsonString()
{
    BmsPackState snapPacks[kTotalPacks];
    char         fault[kFaultReasonLen];
    String       json;

    System_Get_Snapshot(snapPacks);

    /* Copied out under the store mutex rather than read in place: Task_Logic
     * rewrites the buffer every protection scan, and a string is not written
     * atomically. */
    System_Get_FaultReason(fault, sizeof(fault));

    float totalV = System_TotalVoltage(snapPacks);
    float sysI   = snapPacks[0].current;
    int   sysSOC = System_MinSoc(snapPacks);    /* -1 = a slave is offline */

    json.reserve(kJsonReserve);
    json = "{";

    json += "\"packs\":[";

    for (uint8_t i = 0U; i < kTotalPacks; i++)
    {
        json += "{\"volt\":";
        json += String(snapPacks[i].voltage, kJsonDecimals);

        /* Per-pack SOC, computed by the Kalman filter running ON THAT SLAVE
         * and sent back in byte 4. This is the whole point of running five
         * independent filters: it shows WHICH battery is weak. The
         * system-level "soc" below is only their minimum and does not say
         * which one it came from. */
        json += ",\"soc\":";
        json += String(snapPacks[i].soc);

        json += ",\"temp\":";
        json += String(snapPacks[i].temperature);
        json += ",\"err\":";
        json += String(snapPacks[i].status);

        json += ",\"online\":";
        json += snapPacks[i].isConnected ? "true" : "false";
        json += "}";

        if (i < (kTotalPacks - 1U))
        {
            json += ",";
        }
    }

    json += "],";

    json += "\"totalV\":";       json += String(totalV, kJsonDecimals); json += ",";
    json += "\"sysI\":";         json += String(sysI, kJsonDecimals);   json += ",";
    json += "\"soc\":";          json += String(sysSOC);                json += ",";
    json += "\"relayOff\":";     json += System_Get_RelayOverride() ? "true" : "false"; json += ",";
    json += "\"slavesActive\":"; json += System_Get_SlavesActive() ? "true" : "false";  json += ",";
    json += "\"locked\":";       json += systemLocked ? "true" : "false";               json += ",";
    json += "\"fault\":\"";      json += fault;                                         json += "\"";

    json += "}";

    return json;
}
