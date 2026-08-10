/*
 * SOC_Kalman.h
 *
 * Pure 2-state Kalman filter for State-of-Charge (SOC) estimation of ONE
 * lead-acid battery (CSB EVX12200 AGM: ~11.63 V empty .. 12.89 V full).
 *
 * HARDWARE-INDEPENDENT: no HAL, no ADC, no CAN. Only float in / float out.
 * This lets the exact same file be unit-tested on a PC (gcc) and then
 * compiled unchanged for the STM32.
 *
 * State vector x = [ SOC , V_RC ]
 *   SOC  : state of charge, 0.0 (empty) .. 1.0 (full)
 *   V_RC : slow polarization voltage of the 1-RC branch (V)
 *
 * Sign convention: current I > 0 means DISCHARGE (SOC decreases).
 */

#ifndef SOC_KALMAN_H
#define SOC_KALMAN_H

/* ============================================================
 *  BATTERY MODEL PARAMETERS  (tune these in GĐ3 / measure in GĐ8)
 * ============================================================ */

/* --- Battery: CSB EVX12200 (12V 20Ah VRLA-AGM, deep-cycle / E-mobility) ---
 * Datasheet: csb-battery.com, doc RA240531. IEC 60254-1 / UL1989. */

/* --- OCV-SOC linear model: OCV(SOC) = OCV_EMPTY + OCV_SLOPE * SOC ---
 * AGM resting-voltage endpoints (generic AGM SOC chart; to be confirmed by
 * measuring the real battery at rest in GĐ8). */
#define KF_OCV_EMPTY     11.63f     /* OCV at SOC = 0%   (V) */
#define KF_OCV_FULL      12.89f     /* OCV at SOC = 100% (V) */
#define KF_OCV_SLOPE     (KF_OCV_FULL - KF_OCV_EMPTY)  /* = 1.26 V per full SOC */

/* --- Capacity in Coulombs: 20 Ah @20hr-rate (datasheet) * 3600 s/h --- */
#define KF_Q_RATED       72000.0f   /* (A*s) */

/* --- 1-RC equivalent circuit @ 25 C ---
 * R0 from CSB datasheet ("Internal Resistance approx. 13.5 mOhm").
 * R1/C1 are NOT published in any datasheet. Values below are scaled from
 * published pulse-test measurements on a lead-acid battery (Hu et al.,
 * "A Simple Analytical Method for Determining Parameters of Discharging
 * Batteries", IEEE Trans. Energy Conversion — 6V/13Ah cell @60% SOC:
 * R0=65.6mR, R1=70.3mR, C1=247.3F => ratio R1/R0 ~ 1.07, tau ~ 17-37 s;
 * same paper: parameters near-constant for SOC 10-80%).
 * Applied to CSB R0: R1 ~ R0 = 13.5mR, tau ~ 25s -> C1 = tau/R1 ~ 1850F.
 * To be replaced by HPPC measurement on the real battery in GĐ8. */
#define KF_R0_NOM        0.0135f    /* instantaneous ohmic resistance (Ohm) */
#define KF_R1_NOM        0.0135f    /* polarization resistance (Ohm) [IEEE ratio, HPPC pending] */
#define KF_C1_NOM        1850.0f    /* polarization capacitance (F)  [tau~25s, HPPC pending] */

/* --- Temperature correction of R0 (Mức B): R decreases as temp rises --- */
#define KF_TEMP_REF      25.0f      /* reference temperature (C) */
#define KF_ALPHA_R0      0.01f      /* R0 changes ~1%/C around reference */

/* --- Noise tuning (the "who to trust" knobs) --- */
#define KF_Q_SOC         1.0e-7f    /* process noise on SOC (tuned by RMSE/smoothness sweep, GĐ3) */
#define KF_Q_VRC         1.0e-6f    /* process noise on V_RC */
#define KF_R_MEAS        4.0e-4f    /* voltage meas. variance: sigma~20mV (12-bit ADC + 100-avg chain, conservative for un-modeled noise; replace with measured variance in GĐ8) */

/* --- Robustness defenses --- */
/* |I| above this is physically impossible -> reject, hold the last value.
 *
 * 250 A sits above the CSB EVX12200 datasheet Max Discharge Current of 230 A,
 * so every reading the pack can physically produce is ACCEPTED, and below the
 * master's +/-320 A clamp and the int16 A x100 wire range.
 *
 * Deliberately erring high. The old 60 A was set when the vehicle was assumed
 * to draw 50 A; it is now 4 BLDC motors x 25 A = ~100 A peak, so 60 A would
 * have REJECTED the real current and left the filter integrating a stale value
 * - a silent, systematic SOC drift. Letting a glitch through instead costs one
 * 5 ms Predict step: 250 A x 0.005 s / 72000 C = 0.0017% of SOC, and a
 * persistent lie is still caught by the physics validator and innovation gate. */
#define KF_MAX_CURRENT   250.0f
#define KF_GATE_SIGMA    3.0f       /* innovation gating: reject if |y| > 3*sqrt(S) */
#define KF_VALID_I_MIN   10.0f      /* physics validator active only when |I| > this */
#define KF_VALID_RATIO   0.3f       /* if actual sag < 0.3*expected sag -> current is lying */

/* --- Soft clamp margin ---
 * The INTERNAL state may exceed [0,1] by this margin so that noise around
 * a full/empty battery stays symmetric (a hard clamp biases the estimate
 * and makes it stick at 100%). The REPORTED SOC is always clamped to
 * [0,1] by SOC_Kalman_GetSOC(). */
#define KF_SOC_MARGIN    0.01f

/* ============================================================
 *  FILTER STATE
 * ============================================================ */
typedef struct {
    float soc;        /* state[0]: SOC 0.0 .. 1.0                 */
    float v_rc;       /* state[1]: RC polarization voltage (V)    */
    float P[2][2];    /* error covariance matrix                  */
    float i_prev;     /* last accepted current (for sanity limit) */
} KF_State_t;

/* ============================================================
 *  API  (multirate: predict fast on current, update on fresh voltage)
 * ============================================================ */

/* Initialize filter from a resting voltage measurement (I ~ 0 at boot).
 * Inverts the OCV curve to get the starting SOC. */
void SOC_Kalman_Init(KF_State_t *kf, float v_measured);

/* PREDICT step: advance SOC and covariance using current only.
 * Call at the fast current rate (every dt seconds, e.g. 5 ms).
 *   current : pack current from master via CAN (A), >0 = discharge
 *   dt      : time step (s), e.g. 0.005 for 5 ms
 * Runs the sanity limit on current (defense #2) then Coulomb integration. */
void SOC_Kalman_Predict(KF_State_t *kf, float current, float dt);

/* UPDATE step: correct the predicted state using a FRESH voltage sample.
 * Call only when a new voltage measurement is available (e.g. every 50 ms).
 *   v_measured : battery terminal voltage from ADC (V)
 *   current    : current at the moment of the voltage sample (A)
 *   temp       : battery temperature (C), used to adjust R0
 * Runs the physics validator (#4) and innovation gating (#3).
 * Returns the corrected SOC (0.0 .. 1.0). If v_measured is not finite,
 * the state is left unchanged (no update). */
float SOC_Kalman_Update(KF_State_t *kf, float v_measured,
                        float current, float temp);

/* The OFFICIAL SOC to report/transmit: internal state hard-clamped to
 * [0,1]. Use this (not kf->soc directly) wherever SOC leaves the filter. */
float SOC_Kalman_GetSOC(const KF_State_t *kf);

#endif /* SOC_KALMAN_H */
