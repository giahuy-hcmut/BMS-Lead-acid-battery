/**
 * @file      Task_WebServer.h
 * @author    Do Dang Gia Huy
 * @brief     Wi-Fi access point, dashboard page and the live data channel.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * The transport is HTTP plus Server-Sent Events, NOT WebSocket: the data
 * only ever flows from the BMS to the browser, and SSE reconnects by itself
 * without any code on either side. The two control actions travel the other
 * way as ordinary HTTP POSTs.
 */

#ifndef TASK_WEBSERVER_H_
#define TASK_WEBSERVER_H_

/**********************
 *      INCLUDES
 *********************/

#include <Arduino.h>

#include <ESPAsyncWebServer.h>

#include "System_Data.h"

/**********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/** Owns the access point, the HTTP server and the event stream. */
class WebServerManager
{
public:
    /**
     * @brief  Construct the server on port 80 and the event stream.
     */
    WebServerManager();

    /**
     * @brief  Bring up the access point and register the routes.
     * @return true once the server is listening.
     */
    bool init();

    /**
     * @brief  Push one JSON snapshot to every connected browser.
     * @note   Does nothing when nobody is watching, so an idle vehicle does
     *         not serialise a snapshot every period for no reader.
     * @return None.
     */
    void broadcastData();

private:
    /**
     * @brief  Serialise the whole system state as JSON.
     * @return The JSON document.
     */
    String buildJsonString();

    /* Value members, not pointers: both objects live for as long as the
     * firmware runs, so there is nothing to allocate and nothing to free. */
    AsyncWebServer    server_;
    AsyncEventSource  events_;
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  FreeRTOS entry point for the web server.
 * @param[in] pvParameters  Unused, required by the FreeRTOS signature.
 * @return None.
 */
void Task_WebServer_Run(void *pvParameters);

/**********************
 *      MACROS
 **********************/

#endif /*TASK_WEBSERVER_H_*/
