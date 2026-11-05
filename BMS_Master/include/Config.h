/**
 * @file      Config.h
 * @author    Do Dang Gia Huy
 * @brief     Build-time configuration of the BMS central controller.
 * @date      2026-02-10
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Every value here is a typed `constexpr`, not a `#define`, so the compiler
 * checks it and the debugger can resolve it. The only remaining macros are
 * CSV_LOG and VCAN_LOOPBACK, which select code with #ifdef and therefore
 * cannot be anything else.
 *
 * Naming note: headers keep the .h extension instead of the .hpp the style
 * guide asks for, because PlatformIO and the Arduino framework expect .h and
 * renaming would touch every include in the project for no benefit.
 */

#ifndef CONFIG_H_
#define CONFIG_H_

/**********************
 *      INCLUDES
 *********************/

#include <Arduino.h>

#include <stdint.h>

/**********************
 *      DEFINES
 *********************/

/* --- Conditional-compilation switches ---------------------------------
 * These select code, so they must stay macros.
 */

/* Error-measurement mode. Uncomment to make Task_Terminal print CSV instead
 * of the dashboard table, AND to disable Task_VehicleCAN: that task prints
 * "[VCAN] TX stuck" about four lines a second while no vehicle node is
 * connected, which pollutes the CSV file. Comment it out again afterwards. */
/* #define CSV_LOG */

/* Bring-up without any bus: the MCP2515 loops its own frames back. */
/* #define VCAN_LOOPBACK */

/* ======================================================================
 *  1. Battery pack
 * ===================================================================== */

/** Number of batteries in the series string. */
constexpr uint8_t kTotalPacks = 5U;

/** CAN identifier of pack 0; pack i answers on kCanBaseId + i. */
constexpr uint32_t kCanBaseId = 0x103U;

/* ======================================================================
 *  2. Internal CAN bus, master <-> slaves (ESP32 TWAI)
 * ===================================================================== */

constexpr gpio_num_t kPinCanTx = GPIO_NUM_32;
constexpr gpio_num_t kPinCanRx = GPIO_NUM_35;

constexpr uint32_t kCanBaudRate    = 500000U;
constexpr uint8_t  kCanQueueLength = 20U;   /**< frames buffered from the ISR */

/** Master -> slave frame. Carries the pack current AND doubles as the
 * heartbeat: a slave wakes when it arrives and sleeps after 5 s of silence.
 * There is no separate heartbeat frame. The slave calls the same identifier
 * CAN_MASTER_ID in its Board_Config.h. */
constexpr uint32_t kCanMasterId = 0x100U;

/** 200 Hz, which is the rate of the Kalman Predict step on each slave. */
constexpr uint32_t kCanMasterIntervalMs = 5U;

/** Timeout handed to twai_transmit(). */
constexpr uint32_t kCanTxTimeoutMs = 10U;

/* ======================================================================
 *  3. Protection scan and slave timeout
 * ===================================================================== */

/** Period of the Task_Logic safety scan. 10 ms detects an over-current
 * within one period and still leaves the CPU to everything else. */
constexpr uint32_t kProtectionPeriodMs = 10U;

/** No frame from a slave for this long means it is disconnected:
 * System_Get_Snapshot() clears isConnected and Task_Logic opens the relay.
 * The old name was LCD_TIMEOUT, which was never about the LCD - it has
 * always been a CAN timeout, and System_Data.cpp is its main user. */
constexpr uint32_t kSlaveTimeoutMs = 3000U;

/* ======================================================================
 *  4. ESP-NOW link to the handheld remote
 * ===================================================================== */

/** MAC address of the remote, B8:D6:1A:B8:9C:CC. */
constexpr uint8_t kRemoteMac[6] = {0xB8U, 0xD6U, 0x1AU, 0xB8U, 0x9CU, 0xCCU};

/* ======================================================================
 *  5. Web server, Wi-Fi access point
 * ===================================================================== */

constexpr const char *kWifiApSsid = "MURATA_EV_BMS";
constexpr const char *kWifiApPass = "12345677";   /**< at least 8 characters */

/** MUST be 1: ESP-NOW and the access point share the radio, and a mismatched
 * channel kills ESP-NOW. */
constexpr uint8_t kWifiApChannel = 1U;

/** Server-sent-event refresh period. */
constexpr uint32_t kWebUpdateIntervalMs = 500U;

/* ======================================================================
 *  6. Current sensing: INA219 + external shunt
 * ===================================================================== */

/* SOC is no longer computed here: every slave runs its own Kalman filter and
 * reports the result in byte 4 of its CAN frame, and the master takes the
 * minimum through System_MinSoc(). The constants that fed the old Coulomb
 * counter (BATTERY_CAPACITY_AH, VOLTAGE_SYS_*) were deleted with it.
 *
 * Shunt 100 A / 75 mV = 0.75 mOhm, class 0.5. The R100 (0.1 Ohm) resistor on
 * the module HAS BEEN REMOVED: left in place it sits across the two sense
 * wires and draws 0.75 A through a pair meant for microvolts.
 *
 * Measured LOW SIDE, at the pack negative. The INA219 common-mode range is
 * only 0-26 V and cannot take the 60 V of the high side, 72-75 V while
 * charging. The ESP32 ground sits at IN-, on the pack negative side, so the
 * discharge direction stays inside the valid range; regeneration falls
 * outside the specified range (-56 mV at 75 A) but within the absolute
 * maximum of -0.3 V. State that limit in the report.
 *
 * Numbers MEASURED on the bench (Code/BENCH_INA219, 2026-08-12), not taken
 * from a datasheet:
 *   range   320 mV -> +/-427 A   (pinned at exactly 320.000 for 3.27 V diff)
 *   offset  -17.5 uV = -23.3 mA  -> 0.117 %/h of Coulomb drift on 20 Ah
 *   noise   27.2 uV on one sample -> 12.8 mA after the 8-sample filter
 *   step    10 uV = 13.3 mA
 *   gain    within 1% of the meter (datasheet +/-1%, shunt class 0.5, so
 *           +/-1.5% in total)
 */

constexpr uint8_t kIna219I2cAddr = 0x40U;
constexpr uint8_t kPinInaSda     = 21U;
constexpr uint8_t kPinInaScl     = 22U;

constexpr float kShuntFullA  = 100.0f;
constexpr float kShuntFullMv = 75.0f;

/** Measured at bench step 5, AFTER soldering the shunt, so it includes the
 * thermoelectric effect of the joints: soldering moved the offset by 6 uV,
 * which means the joints are clean. */
constexpr float kCurrentZeroMv = -0.0175f;

/** +1 makes a DISCHARGE current come out POSITIVE. It follows the wiring:
 * IN+ is the end the current ENTERS the shunt through while discharging, on
 * the star-ground side.
 *
 * NOT yet confirmed against a real current - there was no load on the bench.
 * It MUST be checked on the first drive: SOC must FALL, byte 5 of 0x200 must
 * read 2 (DISCHARGE), and the current on the web page must be POSITIVE. If
 * not, change this to -1.0f; do NOT rewire.
 *
 * A wrong sign is DANGEROUS: a negative packI makes the test
 * (packI > kMaxDischargeCurrentA) permanently false, which disables the
 * over-current protection completely. It is at least very loud - the SOC
 * climbs while driving - so it can wait, as long as it is on the start-up
 * checklist. */
constexpr float kCurrentSign = +1.0f;

/** Dead band around zero: below this Task_Current forces the current to
 * exactly 0.0. 0.5 A is about 21x the measured noise (12.8 mA after
 * filtering), so it cannot swallow a real signal. Task_VehicleCAN reuses
 * THIS constant as its "at rest" boundary - one number, two readers, no
 * chance of the two drifting apart. */
constexpr float kCurrentIdleBandA = 0.5f;

/** 8 samples x 3 ms = a 24 ms filter window. */
constexpr uint32_t kTaskCurrentPeriodMs = 3U;

/** Consecutive I2C transactions without an ACK before the sensor counts as
 * lost. */
constexpr uint16_t kCurrentFaultLimit = 20U;

/* ======================================================================
 *  7. Protection thresholds and relay control
 * ===================================================================== */

constexpr uint8_t kPinRelayControl = 23U;

/** Most opto-isolated relay modules are active LOW. Invert these two if the
 * module in use is active HIGH. */
constexpr uint8_t kRelayOn  = HIGH;
constexpr uint8_t kRelayOff = LOW;

/** Over-current trip, A.
 * 50 A was the old figure, from when the vehicle was assumed to draw 50 A.
 * It really draws 4 BLDC motors x 25 A, about 100 A peak, so 50 A WOULD HAVE
 * OPENED THE RELAY DURING NORMAL ACCELERATION. 150 A is 1.5x the expected
 * peak and 65% of the 230 A maximum discharge current in the CSB EVX12200
 * datasheet: high enough not to trip for nothing, low enough to stay under
 * the battery limit.
 * TODO confirm against the motor controller current limit once known. */
constexpr float kMaxDischargeCurrentA = 150.0f;

constexpr int kMinSocShutdownPct = 5;   /**< below this the relay opens      */
constexpr int kRecoverySocPct    = 10;  /**< hysteresis for closing it again */

/* ======================================================================
 *  8. Vehicle CAN bus, MCP2515 over SPI
 * ===================================================================== */

/* The ESP32 has only ONE TWAI controller and it already carries the internal
 * master-slave bus, so the vehicle bus needs a second CAN controller. Module
 * HW-184: MCP2515 + TJA1050. The SPI pins are remapped through the GPIO
 * matrix to suit the PCB layout. */

constexpr uint8_t kPinVcanSck  = 27U;
constexpr uint8_t kPinVcanMosi = 26U;
constexpr uint8_t kPinVcanMiso = 25U;
constexpr uint8_t kPinVcanCs   = 33U;
constexpr uint8_t kPinVcanInt  = 14U;

constexpr uint32_t kVcanStatusId = 0x200U;  /**< BMS -> vehicle: system data */
constexpr uint32_t kVcanPackId   = 0x201U;  /**< BMS -> vehicle: one pack    */

constexpr uint32_t kVcanPeriodMs    = 100U;
constexpr uint8_t  kVcanTxFailLimit = 10U;  /**< consecutive fails -> re-init */

/* ======================================================================
 *  9. Shared wire-format constants
 * ===================================================================== */

/* Both CAN paths encode a current the same way, so the constants live in one
 * place instead of being retyped in Task_CAN.cpp and Task_VehicleCAN.cpp. */

/** Clamp applied BEFORE scaling. An int16 in hundredths of an amp spans
 * +/-327.67 A, so an unclamped sensor glitch would wrap the sign and tell
 * the receiver the pack is charging while it is being discharged. */
constexpr float kCurrentClampA = 320.0f;

/** Voltage and current both travel as hundredths of their unit. */
constexpr float kScaleX100 = 100.0f;

/** Mask for extracting one byte of a multi-byte field. */
constexpr uint16_t kByteMask = 0xFFU;

#endif /*CONFIG_H_*/
