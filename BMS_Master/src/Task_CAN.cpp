/**
 * @file      Task_CAN.cpp
 * @author    Do Dang Gia Huy
 * @brief     TWAI driver wrapper, frame decoding, and the RX and TX tasks.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 */

/**********************
 *      INCLUDES
 **********************/

#include "Task_CAN.h"

/**********************
 *      DEFINES
 **********************/

/** Payload length of the master frame; bytes 2-7 are reserved and zero. */
constexpr uint8_t kMasterFrameDlc = 8U;

/* Smallest payload length that still carries each field of a slave frame.
 * Tested rather than assumed, so a short frame from an older build decodes
 * what it can instead of reading past the data it actually sent. */
constexpr uint8_t kDlcWithVoltage = 2U;     /**< bytes 0-1 */
constexpr uint8_t kDlcWithSoc     = 5U;     /**< byte 4    */
constexpr uint8_t kDlcWithStatus  = 7U;     /**< bytes 5-6 */

/** Slave frames carry the temperature offset so it fits in a uint8_t. */
constexpr int16_t kSlaveTempOffsetC = 40;

/** Delay after a driver error before retrying, so a broken bus cannot spin
 * the RX task at 100% CPU. */
constexpr TickType_t kRxErrorBackoff = pdMS_TO_TICKS(10);

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/* ONE object shared by TWO tasks. The ESP-IDF TWAI driver is a singleton -
 * twai_driver_install() may be called only once - so the object has to live
 * at file scope rather than inside either task.
 *
 * No handshake is needed between them: Rx calls init(), and
 * sendCurrentFrame() opens with `if (!isReady_) return;`, so Tx stays quiet
 * by itself until Rx has finished initialising.
 *
 * The constructor only assigns four members and touches no hardware, so it
 * is safe to run before setup() like any other C++ object at file scope. */
static CanManager s_canBus(kPinCanTx, kPinCanRx, kCanBaudRate);

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Record the pins and bit rate; touches no hardware.
 * @param[in] tx    Transmit pin.
 * @param[in] rx    Receive pin.
 * @param[in] baud  Bit rate.
 */
CanManager::CanManager(gpio_num_t tx, gpio_num_t rx, uint32_t baud)
    : txPin_(tx),
      rxPin_(rx),
      baudRate_(baud),
      isReady_(false)
{
}

/**
 * @brief  Install and start the TWAI driver.
 * @return true when the bus is up.
 */
bool CanManager::init()
{
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(txPin_, rxPin_, TWAI_MODE_NORMAL);

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

    if (baudRate_ == 250000U)
    {
        t_config = TWAI_TIMING_CONFIG_250KBITS();
    }
    else if (baudRate_ == 1000000U)
    {
        t_config = TWAI_TIMING_CONFIG_1MBITS();
    }

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK)
    {
        if (twai_start() == ESP_OK)
        {
            isReady_ = true;
            Serial.println("[CAN] Driver started successfully");
            return true;
        }
    }

    Serial.println("[CAN] Driver failed!");
    return false;
}

/**
 * @brief  Read one frame, BLOCKING until one arrives.
 * @param[out] msgOut  Decoded frame.
 * @return true when a frame was decoded.
 */
bool CanManager::readMessage(BmsMessage &msgOut)
{
    twai_message_t rx_msg;

    if (!isReady_)
    {
        return false;
    }

    /* BLOCKS indefinitely. This used to poll with a zero timeout because the
     * SAME task also had to send 0x100 every 5 ms, and blocking here meant
     * the send never got its turn. Transmission moved to Task_CAN_Tx_Run, so
     * blocking is now possible.
     *
     * Underneath, twai_receive() is an xQueueReceive on the queue the
     * driver's ISR feeds. So: no frame means the task is Blocked at 0% CPU;
     * a frame wakes it through the ISR, and because this task is priority 5,
     * the highest on core 1, portYIELD_FROM_ISR switches context at the end
     * of the ISR. Latency in microseconds instead of the up-to-1 ms of the
     * old polling loop. */
    if (twai_receive(&rx_msg, portMAX_DELAY) != ESP_OK)
    {
        return false;
    }

    msgOut.can_id = rx_msg.identifier;

    if (rx_msg.data_length_code >= kDlcWithVoltage)
    {
        uint16_t raw_vol = (uint16_t)(((uint16_t)rx_msg.data[0] << 8) |
                                      (uint16_t)rx_msg.data[1]);
        msgOut.voltage = (float)raw_vol / kScaleX100;
    }
    else
    {
        msgOut.voltage = 0.0f;
    }

    /* Byte 4 = SOC (%) from the slave's Kalman filter.
     *
     * Bytes 2-3 carry the current and are deliberately NOT decoded: that is
     * the master's OWN value coming back from frame 0x100, so reading it
     * would create a second source of truth for something already known.
     * Only worth decoding later as a loopback diagnostic. */
    if (rx_msg.data_length_code >= kDlcWithSoc)
    {
        msgOut.soc = rx_msg.data[4];
    }
    else
    {
        msgOut.soc = 0U;
    }

    if (rx_msg.data_length_code >= kDlcWithStatus)
    {
        msgOut.temperature = (int8_t)((int16_t)rx_msg.data[5] - kSlaveTempOffsetC);
        msgOut.status      = rx_msg.data[6];
    }
    else
    {
        msgOut.temperature = 0;
        msgOut.status      = 0U;
    }

    return true;
}

/**
 * @brief  Transmit the kCanMasterId frame carrying the pack current.
 * @param[in] current_a  Amperes, positive = discharge.
 * @return None.
 */
void CanManager::sendCurrentFrame(float current_a)
{
    twai_status_info_t status;
    float              clamped = current_a;
    int16_t            raw;

    if (!isReady_)
    {
        return;
    }

    twai_get_status_info(&status);

    if (status.state == TWAI_STATE_BUS_OFF)
    {
        twai_initiate_recovery();
        return;
    }

    if (status.state == TWAI_STATE_STOPPED)
    {
        twai_start();
        return;
    }

    if (status.state == TWAI_STATE_RECOVERING)
    {
        return;
    }

    /* Clamp before scaling: an int16 in hundredths of an amp spans
     * +/-327.67 A. A wild sensor reading would otherwise wrap sign and tell
     * every slave the pack is charging while it is really being discharged -
     * the SOC would climb under load with no fault raised anywhere. */
    if (clamped > kCurrentClampA)
    {
        clamped = kCurrentClampA;
    }

    if (clamped < -kCurrentClampA)
    {
        clamped = -kCurrentClampA;
    }

    raw = (int16_t)lroundf(clamped * kScaleX100);

    /* `= {}` zero-fills data[], so bytes 2-7 go out as 0x00 without being
     * written again below. */
    twai_message_t msg = {};
    msg.identifier = kCanMasterId;

    /* DLC 8 in BOTH directions, the J1939 convention: a fixed frame size
     * means adding a field later only fills a reserved byte instead of
     * changing the DLC, which would break every receiver that tests it.
     * Bytes 2-7 are RESERVED and must stay 0x00 until a field is defined
     * here AND in the project brief.
     * Cost: 75 -> 135 bits worst case including stuffing, 150 -> 270 us at
     * 500 kbit/s. At a 5 ms period that is 3.0% -> 5.4% bus load, and the
     * blocking time other nodes see grows by the same 120 us. */
    msg.data_length_code = kMasterFrameDlc;
    msg.data[0] = (uint8_t)(((uint16_t)raw >> 8) & kByteMask);  /* MSB first, */
    msg.data[1] = (uint8_t)((uint16_t)raw & kByteMask);         /* as in a slave frame */

    twai_transmit(&msg, pdMS_TO_TICKS(kCanTxTimeoutMs));
}

/**
 * @brief  Receive task: blocks on the bus and queues each decoded frame.
 * @param[in] pvParameters  Unused.
 * @return None.
 */
void Task_CAN_Rx_Run(void *pvParameters)
{
    BmsMessage tempMsg;

    (void)pvParameters;

    if (!s_canBus.init())
    {
        Serial.println("[CAN] Init failed -> delete Rx task");
        vTaskDelete(NULL);
    }

    for (;;)
    {
        if (s_canBus.readMessage(tempMsg))       /* BLOCKS here */
        {
            xQueueSend(canQueue, &tempMsg, 0);
        }
        else
        {
            /* portMAX_DELAY never times out, so false means a DRIVER ERROR:
             * recovering from bus-off, or the driver stopped. Without this
             * delay the loop spins at 100% CPU for as long as the bus stays
             * broken.
             *
             * Tx is the side that calls twai_initiate_recovery(), since
             * bus-off is detected from the transmitting end, so the two
             * tasks co-operate: Tx repairs the bus, Rx waits. */
            vTaskDelay(kRxErrorBackoff);
        }
    }
}

/**
 * @brief  Transmit task: sends the current frame on a fixed period.
 * @param[in] pvParameters  Unused.
 * @return None.
 */
void Task_CAN_Tx_Run(void *pvParameters)
{
    /* vTaskDelayUntil, NOT vTaskDelay: the latter measures the period from
     * when the work FINISHES, so the execution time is added to every cycle
     * and the cadence drifts. The older approach, vTaskDelay(1 ms) plus a
     * tick count, also quantised the period to the 1 ms tick and produced
     * 6 ms instead of 5 now and then. Task_VehicleCAN uses DelayUntil for
     * the same reason. */
    TickType_t last = xTaskGetTickCount();

    (void)pvParameters;

    for (;;)
    {
        /* System_Get_SlavesActive() stays: switching it off from the web is
         * still how the slaves are put to sleep, because 0x100 doubles as
         * their heartbeat. */
        if (System_Get_SlavesActive())
        {
            s_canBus.sendCurrentFrame(System_Get_Current());
        }

        vTaskDelayUntil(&last, pdMS_TO_TICKS(kCanMasterIntervalMs));
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
