/**
 * @file      BMS_CAN.h
 * @author    Do Dang Gia Huy
 * @brief     CAN transport for the slave: filtered RX and one-frame TX.
 * @date      2026-01-31
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * The driver owns the CAN handle, so no layer above it has to name a HAL
 * type. Reception is interrupt-driven and hardware-filtered down to the
 * master frame alone.
 */

#ifndef BMS_CAN_H_
#define BMS_CAN_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      INCLUDES
 *********************/

#include "stm32f1xx_hal.h"

/**********************
 *      DEFINES
 *********************/

/* Return values of BMS_CAN_Transmit(). */
#define BMS_CAN_TX_OK       1U
#define BMS_CAN_TX_BUSY     0U

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  Hand the CAN handle to the driver and preset the TX header.
 * @param  hcan  Handle configured by MX_CAN_Init().
 * @note   Call once at start-up, before any other function of this module.
 * @return None.
 */
void BMS_CAN_Init(CAN_HandleTypeDef *hcan);

/**
 * @brief  Install the acceptance filter and enable the RX FIFO 0 interrupt.
 * @note   Uses the handle given to BMS_CAN_Init(); does nothing if that call
 *         was missed.
 * @return None.
 */
void BMS_CAN_InitRx(void);

/**
 * @brief  Queue one standard data frame for transmission.
 * @param  id    11-bit standard identifier.
 * @param  data  Payload, at least @p len bytes.
 * @param  len   Payload length, 0..8.
 * @note   Non-blocking and single-shot: if no mailbox is free the frame is
 *         dropped rather than queued, which is what the fixed task period
 *         expects.
 * @return BMS_CAN_TX_OK when the frame entered a mailbox, BMS_CAN_TX_BUSY on
 *         a full mailbox, a HAL error, or an uninitialised driver.
 */
uint8_t BMS_CAN_Transmit(uint32_t id, uint8_t *data, uint8_t len);

/**
 * @brief  HAL_GetTick() value of the most recent frame from the master.
 * @note   Task_Sleep uses it as the master heartbeat timeout.
 * @return Tick of the last accepted master frame, 0 if none arrived yet.
 */
uint32_t BMS_CAN_GetLastRxTick(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*BMS_CAN_H_*/
