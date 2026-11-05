/**
 * @file      Board_Config.h
 * @author    Do Dang Gia Huy
 * @brief     Per-board identity and protection limits for ONE slave node.
 * @date      2026-08-04
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * This is the only header that differs between the five slaves.
 */

#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      INCLUDES
 *********************/

/**********************
 *      DEFINES
 *********************/

/* --- Slave identity --- */

/** The ONLY line to edit when flashing a different board: 0..4, one slave
 *  per battery. */
#define SLAVE_INDEX             0U

#if (SLAVE_INDEX > 4U)
    #error "SLAVE_INDEX must be 0..4"
#endif

/* --- CAN addressing, derived from SLAVE_INDEX; do not edit --- */
#define CAN_BASE_ID             0x103U                      /**< id of slave 0  */
#define CAN_SLAVE_ID            (CAN_BASE_ID + SLAVE_INDEX) /**< this node's TX */
#define CAN_MASTER_ID           0x100U                      /**< master->slaves */

/* --- Protection limits ---
 * Generic 12 V lead-acid AGM limits, deliberately NOT tied to one part number
 * since the pack is expected to change. They match the common market band,
 * and also the CSB EVX12200 datasheet currently on the bench, rev RA240531:
 *
 *   cycle charge voltage   14.4 - 14.8 V typical (CSB: 14.4 - 15.0 V at 25 C)
 *   final discharge (F.V)  10.5 V = 1.75 V/cell, the 20 Ah / 20 h rating point
 *   operating temperature  up to 50 C on discharge
 */

/** Over-voltage, V. Trips ABOVE the charge band: the old 12.7 V sat below
 *  even the minimum charge voltage, so it raised a fault for the whole charge
 *  cycle. */
#define THRESHOLD_OVER_VOLT     15.00f

/** Under-voltage, V. The old 9.00 V allowed a much deeper discharge than any
 *  AGM datasheet does.
 *
 *  10.5 V is a RESTING figure, while the ADC reads the terminal voltage. At
 *  100 A the I*R drop is 100 * 13.5 mOhm = 1.35 V, so a battery resting at
 *  11.85 V would read 10.5 V under load and trip at about 17% SOC.
 *  Task_Voltage therefore compares the IR-compensated value V + I*R0 against
 *  this threshold, using the current the master publishes over CAN, and falls
 *  back to the raw voltage when that current is stale. */
#define THRESHOLD_UNDER_VOLT    10.50f

/** Over-temperature, C. The datasheet discharge limit is 50 C. Note the
 *  DS18B20 reads the CASE while the core runs hotter under load, so this is
 *  not a conservative figure. */
#define THRESHOLD_OVER_TEMP     50.0f

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*BOARD_CONFIG_H_*/
