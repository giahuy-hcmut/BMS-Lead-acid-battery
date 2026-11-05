/**
 * @file      Task_CAN.h
 * @author    Do Dang Gia Huy
 * @brief     Driver and tasks for the internal CAN bus, master <-> slaves.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * TWO tasks, because reception and transmission are different in kind, and
 * merging them is exactly why the earlier version had to poll:
 *   Rx  EVENT driven, blocks in twai_receive(), 0% CPU while the bus is idle
 *   Tx  PERIODIC, vTaskDelayUntil, so the 0x100 cadence never drifts
 *
 * In one task the two fight each other: blocking to receive means the send
 * never gets its turn, and sending on time means polling a thousand times a
 * second, about 995 of which find nothing.
 */

#ifndef TASK_CAN_H_
#define TASK_CAN_H_

/**********************
 *      INCLUDES
 *********************/

#include <Arduino.h>

#include "driver/twai.h"

#include "System_Data.h"    /* BmsMessage */

/**********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/** Thin wrapper over the ESP32 TWAI peripheral: the hardware layer of the
 * internal bus. */
class CanManager
{
public:
    /**
     * @brief  Record the pins and bit rate; touches no hardware.
     * @param[in] tx    Transmit pin.
     * @param[in] rx    Receive pin.
     * @param[in] baud  Bit rate; 250k, 500k and 1M are supported.
     * @note   Safe to run before setup(), as a file-scope object does.
     */
    CanManager(gpio_num_t tx, gpio_num_t rx, uint32_t baud = kCanBaudRate);

    /**
     * @brief  Install and start the TWAI driver.
     * @return true when the bus is up.
     */
    bool init();

    /**
     * @brief  Read one frame, BLOCKING until one arrives.
     * @param[out] msgOut  Decoded frame.
     * @note   With portMAX_DELAY twai_receive() never times out, so a false
     *         return does NOT mean "no frame yet": it means a DRIVER ERROR,
     *         the bus being stopped or recovering from bus-off. The caller
     *         MUST delay on false, or it spins at 100% CPU for as long as
     *         the bus stays broken.
     * @return true when a frame was decoded.
     */
    bool readMessage(BmsMessage &msgOut);

    /**
     * @brief  Transmit the kCanMasterId frame carrying the pack current.
     * @param[in] current_a  Amperes, positive = DISCHARGE, the convention
     *                       used throughout the system.
     * @note   Encoded as int16 two's complement, MSB first, in hundredths of
     *         an amp - the same layout as bytes 2-3 of a slave frame.
     * @return None.
     */
    void sendCurrentFrame(float current_a);

private:
    gpio_num_t txPin_;
    gpio_num_t rxPin_;
    uint32_t   baudRate_;
    bool       isReady_;
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  Receive task: blocks on the bus and queues each decoded frame.
 * @param[in] pvParameters  Unused, required by the FreeRTOS signature.
 * @return None.
 */
void Task_CAN_Rx_Run(void *pvParameters);

/**
 * @brief  Transmit task: sends the current frame on a fixed period.
 * @param[in] pvParameters  Unused, required by the FreeRTOS signature.
 * @return None.
 */
void Task_CAN_Tx_Run(void *pvParameters);

/**********************
 *      MACROS
 **********************/

#endif /*TASK_CAN_H_*/
