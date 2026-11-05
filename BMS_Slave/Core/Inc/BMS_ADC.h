/**
 * @file      BMS_ADC.h
 * @author    Do Dang Gia Huy
 * @brief     Battery terminal voltage acquisition through ADC1_IN9.
 * @date      2026-01-31
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * The driver owns the ADC handle, so no layer above it has to name a HAL
 * type. It deliberately does NOT include Shared_Data.h: a driver must not
 * depend on the data layer.
 */

#ifndef BMS_ADC_H_
#define BMS_ADC_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      INCLUDES
 *********************/

#include "stm32f1xx_hal.h"

#include "Board_Config.h"   /* SLAVE_INDEX only */

/**********************
 *      DEFINES
 *********************/

/* --- Front end ---
 * Resistive divider: R1 to V_BAT, R2 to GND, mid-node read on PB1 (ADC1_IN9).
 *   V_pin      = V_BAT * R2 / (R1 + R2)
 *   V_computed = V_pin * (R1 + R2) / R2      (invert the divider)
 * The same 33k/9.1k pair is fitted on all five slaves. Ratio 0.2162 puts a
 * 14 V battery at 3.03 V, safely under the 3.3 V reference, and the source
 * impedance 33k||9.1k = 7.13k suits a 71.5-cycle sample time. */
#define ADC_VREF            3.3f
#define ADC_RESOLUTION      4095.0f
#define ADC_NUM_SAMPLES     100U

#define ADC_R1_VAL          33000.0f    /* Ohm, to V_BAT */
#define ADC_R2_VAL          9100.0f     /* Ohm, to GND   */

/* --- Two-point calibration, per slave ---
 *   V_true = CALIB_K * V_computed + CALIB_B
 * Correcting both slope and offset is more accurate than a single gain.
 * Procedure: measure two points against a meter, then
 *   K = (V2 - V1) / (Vc2 - Vc1),  B = V1 - K * Vc1.
 * K = 1.0 with B = 0.0 means NOT YET CALIBRATED, which is also the identity
 * needed while running the calibration itself. */
#if   (SLAVE_INDEX == 0U)   /* CAN 0x103 */
    #define CALIB_K     0.984f      /* points: (8.95 -> 9.16), (11.70 -> 11.83) */
    #define CALIB_B     -0.0046f
#elif (SLAVE_INDEX == 1U)   /* CAN 0x104 */
    #define CALIB_K     0.9935f
    #define CALIB_B     -0.02f
#elif (SLAVE_INDEX == 2U)   /* CAN 0x105 */
    #define CALIB_K     1.05428f
    #define CALIB_B     -0.742f
#elif (SLAVE_INDEX == 3U)   /* CAN 0x106 */
    #define CALIB_K     1.0f        /* TODO measure */
    #define CALIB_B     0.0f        /* TODO measure */
#elif (SLAVE_INDEX == 4U)   /* CAN 0x107 */
    #define CALIB_K     1.0f        /* TODO measure */
    #define CALIB_B     0.0f        /* TODO measure */
#else
    #error "No calibration constants for this SLAVE_INDEX"
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  Hand the ADC handle to the driver.
 * @param  hadc  Handle configured by MX_ADC1_Init(); may not be NULL in
 *               normal operation, see BMS_ADC_GetVoltage() for the NULL case.
 * @note   Call once at start-up, before BMS_ADC_GetVoltage().
 * @return None.
 */
void BMS_ADC_Init(ADC_HandleTypeDef *hadc);

/**
 * @brief  Measure the battery terminal voltage, calibration applied.
 * @note   Blocking: averages ADC_NUM_SAMPLES conversions, about 0.7 ms in
 *         total at a 12 MHz ADC clock with a 71.5-cycle sample time.
 * @return Terminal voltage in volts, or 0.0f when BMS_ADC_Init() was never
 *         called or when not a single conversion completed. 0.0f is below the
 *         under-voltage threshold on purpose, so a broken driver raises a
 *         fault instead of reporting a plausible value.
 */
float BMS_ADC_GetVoltage(void);

/* --- Calibration aids ---
 * A deliberate exception to the encapsulation rule: these must have external
 * linkage for the debugger to resolve them in Live Expressions. Read-only,
 * and no control path depends on them. */
extern volatile float g_dbg_adc_avg;        /**< Mean of the raw samples, 0..4095 */
extern volatile float g_dbg_v_computed;     /**< Voltage before calibration, V    */

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*BMS_ADC_H_*/
