/**
 * @file      SOC_Kalman.h
 * @author    Do Dang Gia Huy
 * @brief     Two-state Kalman state-of-charge estimator for one lead-acid
 *            battery.
 * @date      2026-08-06
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Target cell: CSB EVX12200 AGM, about 11.63 V empty to 12.89 V full.
 *
 * HARDWARE INDEPENDENT: no HAL, no ADC, no CAN, only float in and float out.
 * The same file therefore compiles unchanged for the host (gcc, unit tests)
 * and for the STM32.
 *
 * State vector x = [ SOC , V_RC ]
 *   SOC   state of charge, 0.0 empty .. 1.0 full
 *   V_RC  slow polarisation voltage of the 1-RC branch, V
 *
 * Sign convention: current I > 0 means DISCHARGE, so SOC decreases.
 *
 * Matrix symbols P, R0 and S keep their upper-case names from the Kalman
 * literature and from the project report rather than the lower-case form of
 * the coding standard, so the code can be read against the equations.
 */

#ifndef SOC_KALMAN_H_
#define SOC_KALMAN_H_

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *      INCLUDES
 *********************/

/**********************
 *      DEFINES
 *********************/

/* ---------------------------------------------------------------------
 * Battery: CSB EVX12200, 12 V 20 Ah VRLA-AGM, deep cycle / e-mobility.
 * Datasheet csb-battery.com, document RA240531, IEC 60254-1 / UL 1989.
 * ------------------------------------------------------------------ */

/* OCV-SOC linear model: OCV(SOC) = KF_OCV_EMPTY + KF_OCV_SLOPE * SOC.
 * AGM resting-voltage endpoints from a generic AGM SOC chart, to be
 * confirmed against the real battery at rest. */
#define KF_OCV_EMPTY     11.63f     /**< OCV at SOC = 0%, V   */
#define KF_OCV_FULL      12.89f     /**< OCV at SOC = 100%, V */
#define KF_OCV_SLOPE     (KF_OCV_FULL - KF_OCV_EMPTY)   /**< 1.26 V per full SOC */

/** Capacity in coulombs: 20 Ah at the 20 h rate (datasheet) x 3600 s/h. */
#define KF_Q_RATED       72000.0f

/* 1-RC equivalent circuit at 25 C.
 * R0 comes from the CSB datasheet, "Internal Resistance approx. 13.5 mOhm".
 * R1 and C1 are NOT published in any datasheet. The values below are scaled
 * from published pulse-test measurements on a lead-acid battery: Hu et al.,
 * "A Simple Analytical Method for Determining Parameters of Discharging
 * Batteries", IEEE Trans. Energy Conversion - a 6 V / 13 Ah cell at 60% SOC
 * gives R0 = 65.6 mOhm, R1 = 70.3 mOhm, C1 = 247.3 F, so R1/R0 is about 1.07
 * and tau about 17-37 s; the same paper reports the parameters are nearly
 * constant over SOC 10-80%.
 * Applied to the CSB R0: R1 ~ R0 = 13.5 mOhm, tau ~ 25 s -> C1 ~ 1850 F.
 * To be replaced by an HPPC measurement on the real battery. */
#define KF_R0_NOM        0.0135f    /**< Instantaneous ohmic resistance, Ohm */
#define KF_R1_NOM        0.0135f    /**< Polarisation resistance, Ohm        */
#define KF_C1_NOM        1850.0f    /**< Polarisation capacitance, F         */

/* Temperature correction of R0: resistance falls as temperature rises. */
#define KF_TEMP_REF      25.0f      /**< Reference temperature, C       */
#define KF_ALPHA_R0      0.01f      /**< R0 changes about 1%/C about it */

/* Noise tuning, the "who do we trust" knobs. */
#define KF_Q_SOC         1.0e-7f    /**< Process noise on SOC, from an RMSE
                                     *   and smoothness sweep             */
#define KF_Q_VRC         1.0e-6f    /**< Process noise on V_RC            */
#define KF_R_MEAS        4.0e-4f    /**< Voltage measurement variance,
                                     *   sigma about 20 mV for the 12-bit
                                     *   ADC plus 100-sample average;
                                     *   conservative to cover unmodelled
                                     *   noise, replace with the measured
                                     *   variance once available          */

/* ---------------------------------------------------------------------
 * Robustness defences
 * ------------------------------------------------------------------ */

/* A current above this is physically impossible -> reject it and hold the
 * last accepted value.
 *
 * 250 A sits above the CSB EVX12200 datasheet maximum discharge current of
 * 230 A, so every reading the pack can physically produce is ACCEPTED, and it
 * stays below the master's +/-320 A clamp and the int16 A x100 wire range.
 *
 * Deliberately erring high. The old 60 A was set when the vehicle was assumed
 * to draw 50 A; it is now 4 BLDC motors x 25 A, about 100 A peak, so 60 A
 * would have REJECTED the real current and left the filter integrating a
 * stale value - a silent, systematic SOC drift. Letting a glitch through
 * instead costs one 5 ms Predict step: 250 A x 0.005 s / 72000 C = 0.0017% of
 * SOC, and a persistent lie is still caught by the physics validator and the
 * innovation gate. */
#define KF_MAX_CURRENT   250.0f

#define KF_GATE_SIGMA    3.0f       /**< Innovation gate: reject when
                                     *   |y| > 3 * sqrt(S)                */
#define KF_VALID_I_MIN   10.0f      /**< Physics validator runs only when
                                     *   |I| exceeds this, A              */
#define KF_VALID_RATIO   0.3f       /**< Actual sag below this fraction of
                                     *   the expected sag means the
                                     *   current reading is lying         */

/* Soft clamp margin.
 * The INTERNAL state may exceed [0,1] by this margin so that noise around a
 * full or empty battery stays symmetric; a hard clamp biases the estimate and
 * makes the SOC stick at 100%. The REPORTED SOC is always clamped to [0,1] by
 * SOC_Kalman_GetSOC(). */
#define KF_SOC_MARGIN    0.01f

/**********************
 *      TYPEDEFS
 **********************/

/** Complete filter state; one instance per battery. */
typedef struct
{
    float soc;          /**< state[0]: SOC, 0.0 .. 1.0                   */
    float v_rc;         /**< state[1]: RC polarisation voltage, V        */
    float P[2][2];      /**< Error covariance matrix                     */
    float i_prev;       /**< Last accepted current, for the sanity limit */
} KF_State_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief  Initialise the filter from a resting voltage measurement.
 * @param  p_kf        Filter state to initialise.
 * @param  v_measured  Terminal voltage at rest, V.
 * @note   At boot the battery rests (I about 0), so the terminal voltage is
 *         the OCV and the curve can be inverted for the starting SOC.
 * @return None.
 */
void SOC_Kalman_Init(KF_State_t *p_kf, float v_measured);

/**
 * @brief  Predict step: advance SOC and covariance from the current alone.
 * @param  p_kf     Filter state.
 * @param  current  Pack current from the master, A, positive = discharge.
 * @param  dt       Time step in seconds, for example 0.005 for 5 ms.
 * @note   Call at the fast current rate. Applies the sanity limit on the
 *         current before the Coulomb integration.
 * @return None.
 */
void SOC_Kalman_Predict(KF_State_t *p_kf, float current, float dt);

/**
 * @brief  Update step: correct the predicted state with a FRESH voltage.
 * @param  p_kf        Filter state.
 * @param  v_measured  Terminal voltage from the ADC, V.
 * @param  current     Current at the instant of that voltage sample, A.
 * @param  temp        Battery temperature in C, used to adjust R0.
 * @note   Call only when a new voltage sample exists, about every 50 ms.
 *         Runs the physics validator and the innovation gate. A non-finite
 *         v_measured leaves the state untouched.
 * @return The corrected SOC, 0.0 .. 1.0.
 */
float SOC_Kalman_Update(KF_State_t *p_kf, float v_measured, float current, float temp);

/**
 * @brief  The SOC to report or transmit.
 * @param  p_kf  Filter state.
 * @note   Use this rather than reading p_kf->soc: the internal state is
 *         allowed to overshoot [0,1] by KF_SOC_MARGIN, the reported value
 *         never is.
 * @return Internal SOC hard-clamped to [0,1].
 */
float SOC_Kalman_GetSOC(const KF_State_t *p_kf);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*SOC_KALMAN_H_*/
