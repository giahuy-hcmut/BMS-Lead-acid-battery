/**
 * @file      SOC_Kalman.c
 * @author    Do Dang Gia Huy
 * @brief     Multirate two-state Kalman filter for state-of-charge.
 * @date      2026-08-06
 * @copyright Copyright (c) 2026 Do Dang Gia Huy. All rights reserved.
 *
 * Multirate design:
 *   - SOC_Kalman_Predict() runs at the fast current rate, 5 ms;
 *   - SOC_Kalman_Update() runs only when a fresh voltage sample exists,
 *     about every 50 ms.
 * The split is what keeps the filter from ever correcting with a stale
 * voltage reading.
 *
 * All matrix algebra is expanded to scalars, 2x2 by hand, so no linear
 * algebra library is needed and one Update takes about 20 us on an
 * STM32F103 without an FPU.
 */

/**********************
 *      INCLUDES
 **********************/

#include "SOC_Kalman.h"

#include <math.h>

/**********************
 *      DEFINES
 **********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static float kf_ocv_from_soc(float soc);
static float kf_clamp_soc(float soc);
static float kf_sanitize_current(KF_State_t *p_kf, float current);

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
 * @brief  Initialise the filter from a resting voltage measurement.
 * @param  p_kf        Filter state to initialise.
 * @param  v_measured  Terminal voltage at rest, V.
 * @return None.
 */
void SOC_Kalman_Init(KF_State_t *p_kf, float v_measured)
{
    float soc0;

    /* At boot the battery rests (I about 0), so the terminal voltage is the
     * OCV. Invert OCV(SOC): SOC = (V - OCV_EMPTY) / OCV_SLOPE.
     * HARD clamp here: the soft margin exists only to keep run-time noise
     * symmetric, while a one-shot boot estimate must start inside [0,1]. */
    soc0 = (v_measured - KF_OCV_EMPTY) / KF_OCV_SLOPE;

    if (soc0 > 1.0f)
    {
        soc0 = 1.0f;
    }

    if (soc0 < 0.0f)
    {
        soc0 = 0.0f;
    }

    p_kf->soc  = soc0;
    p_kf->v_rc = 0.0f;

    /* Start "unsure" so the filter leans on the measurements early on, then
     * converges as the covariance shrinks. */
    p_kf->P[0][0] = 1.0f;
    p_kf->P[0][1] = 0.0f;
    p_kf->P[1][0] = 0.0f;
    p_kf->P[1][1] = 1.0f;

    p_kf->i_prev = 0.0f;
}

/**
 * @brief  Predict step: advance SOC and covariance from the current alone.
 * @param  p_kf     Filter state.
 * @param  current  Pack current, A, positive = discharge.
 * @param  dt       Time step, s.
 * @return None.
 */
void SOC_Kalman_Predict(KF_State_t *p_kf, float current, float dt)
{
    float i_in;
    float a;
    float b;
    float soc_p;
    float vrc_p;
    float p00;
    float p01;
    float p10;
    float p11;

    i_in = kf_sanitize_current(p_kf, current);      /* defence #2 */
    p_kf->i_prev = i_in;

    /* State prediction:
     *   SOC drops by the charge pulled out during this step,
     *   V_RC relaxes towards its steady value through the RC time constant. */
    a = 1.0f - (dt / (KF_R1_NOM * KF_C1_NOM));      /* RC decay factor */
    b = dt / KF_C1_NOM;

    soc_p = p_kf->soc - ((dt / KF_Q_RATED) * i_in);
    vrc_p = (a * p_kf->v_rc) + (b * i_in);

    /* Covariance prediction P = A*P*A' + Q, with A = [[1,0],[0,a]]. */
    p00 = p_kf->P[0][0] + KF_Q_SOC;
    p01 = a * p_kf->P[0][1];
    p10 = a * p_kf->P[1][0];
    p11 = (a * a * p_kf->P[1][1]) + KF_Q_VRC;

    p_kf->soc  = kf_clamp_soc(soc_p);
    p_kf->v_rc = vrc_p;

    p_kf->P[0][0] = p00;
    p_kf->P[0][1] = p01;
    p_kf->P[1][0] = p10;
    p_kf->P[1][1] = p11;
}

/**
 * @brief  Update step: correct the predicted state with a FRESH voltage.
 * @param  p_kf        Filter state.
 * @param  v_measured  Terminal voltage from the ADC, V.
 * @param  current     Current at the instant of that voltage sample, A.
 * @param  temp        Battery temperature, C.
 * @return The corrected SOC, 0.0 .. 1.0.
 */
float SOC_Kalman_Update(KF_State_t *p_kf, float v_measured, float current, float temp)
{
    /* Measurement Jacobian H = d(v_pred)/d(state) = [OCV_SLOPE, -1]. */
    const float h0 = KF_OCV_SLOPE;
    const float h1 = -1.0f;

    float i_in;
    float R0;
    float ocv_est;
    float expected_sag;
    float actual_sag;
    float v_pred;
    float innovation;
    float p00;
    float p01;
    float p10;
    float p11;
    float ph0;
    float ph1;
    float S;
    float k0;
    float k1;
    float soc_new;
    float vrc_new;
    float n00;
    float n01;
    float n10;
    float n11;

    /* No update without a valid measurement (defence #9). */
    if (!isfinite(v_measured))
    {
        return SOC_Kalman_GetSOC(p_kf);
    }

    i_in = kf_sanitize_current(p_kf, current);      /* defence #2 */

    /* Temperature-adjusted ohmic resistance: colder means a higher R0. */
    R0 = KF_R0_NOM * (1.0f + (KF_ALPHA_R0 * (KF_TEMP_REF - temp)));

    /* Defence #4: physics validator, DISCHARGE ONLY.
     * A large discharge current MUST sag the terminal voltage. If a big
     * positive current is reported but the voltage barely sagged, the sensor
     * is lying (stuck or offset), so zero it in the measurement model.
     * Deliberately NOT applied to regeneration (I < 0): with the polarisation
     * voltage V_RC still decaying after a load phase, an OCV-based sag test
     * misreads legitimate charging as a fault, which caused SOC spikes at
     * every ECE-15 braking event. Corrupt regen readings are still caught by
     * the innovation gate (#3) and the sanity limit (#2). */
    if (i_in > KF_VALID_I_MIN)
    {
        ocv_est      = kf_ocv_from_soc(p_kf->soc);
        expected_sag = i_in * R0;
        actual_sag   = ocv_est - v_measured;        /* > 0 while discharging */

        if (actual_sag < (KF_VALID_RATIO * expected_sag))
        {
            i_in = 0.0f;
        }
    }

    p_kf->i_prev = i_in;

    /* Terminal voltage predicted from the already predicted state. */
    v_pred     = kf_ocv_from_soc(p_kf->soc) - (i_in * R0) - p_kf->v_rc;
    innovation = v_measured - v_pred;

    /* Innovation covariance S = H*P*H' + R, a scalar. R > 0 so S > 0. */
    p00 = p_kf->P[0][0];
    p01 = p_kf->P[0][1];
    p10 = p_kf->P[1][0];
    p11 = p_kf->P[1][1];

    ph0 = (p00 * h0) + (p01 * h1);      /* (P*H')[0] */
    ph1 = (p10 * h0) + (p11 * h1);      /* (P*H')[1] */
    S   = (h0 * ph0) + (h1 * ph1) + KF_R_MEAS;

    /* Defence #3: innovation gating. A mismatch far larger than statistically
     * expected means the measurement is corrupt, so skip the correction.
     *
     * Compared as squares rather than |y| > k*sqrt(S). The two are equivalent
     * because both sides are non-negative and S > 0 always (S = H*P*H' + R
     * with R = KF_R_MEAS > 0). Avoiding sqrtf() keeps libm out of the
     * firmware link, which measured 308 bytes of flash and 392 bytes of RAM
     * on the STM32F103 - RAM being the scarce resource at 20 KB - and it
     * drops a 100 to 300 cycle soft-float call from every Update. */
    if ((innovation * innovation) > (KF_GATE_SIGMA * KF_GATE_SIGMA * S))
    {
        return SOC_Kalman_GetSOC(p_kf);
    }

    /* Kalman gain K = P*H' / S, a 2x1 vector. */
    k0 = ph0 / S;
    k1 = ph1 / S;

    /* Correct the state by the gained innovation. */
    soc_new = p_kf->soc  + (k0 * innovation);
    vrc_new = p_kf->v_rc + (k1 * innovation);

    /* Covariance update P = (I - K*H) * P. */
    n00 = ((1.0f - (k0 * h0)) * p00) + ((-k0 * h1) * p10);
    n01 = ((1.0f - (k0 * h0)) * p01) + ((-k0 * h1) * p11);
    n10 = ((-k1 * h0) * p00) + ((1.0f - (k1 * h1)) * p10);
    n11 = ((-k1 * h0) * p01) + ((1.0f - (k1 * h1)) * p11);

    p_kf->soc  = kf_clamp_soc(soc_new);
    p_kf->v_rc = vrc_new;

    p_kf->P[0][0] = n00;
    p_kf->P[0][1] = n01;
    p_kf->P[1][0] = n10;
    p_kf->P[1][1] = n11;

    return SOC_Kalman_GetSOC(p_kf);
}

/**
 * @brief  The SOC to report or transmit.
 * @param  p_kf  Filter state.
 * @return Internal SOC hard-clamped to [0,1].
 */
float SOC_Kalman_GetSOC(const KF_State_t *p_kf)
{
    float soc = p_kf->soc;

    if (soc > 1.0f)
    {
        soc = 1.0f;
    }

    if (soc < 0.0f)
    {
        soc = 0.0f;
    }

    return soc;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  Open-circuit voltage of the linear OCV-SOC model.
 * @param  soc  State of charge, 0.0 .. 1.0.
 * @return OCV in volts.
 */
static float kf_ocv_from_soc(float soc)
{
    return KF_OCV_EMPTY + (KF_OCV_SLOPE * soc);
}

/**
 * @brief  Soft clamp applied to the INTERNAL state.
 * @param  soc  Candidate state of charge.
 * @note   A small overshoot beyond [0,1] is allowed so that measurement
 *         noise around a full or empty battery stays symmetric. A hard clamp
 *         here would bias the estimate upward at 100%, since noise pushing up
 *         is cut while noise pushing down is kept, and the SOC would stick at
 *         full.
 * @return The state of charge limited to [-KF_SOC_MARGIN, 1 + KF_SOC_MARGIN].
 */
static float kf_clamp_soc(float soc)
{
    float limited = soc;

    if (limited > (1.0f + KF_SOC_MARGIN))
    {
        limited = 1.0f + KF_SOC_MARGIN;
    }

    if (limited < (0.0f - KF_SOC_MARGIN))
    {
        limited = 0.0f - KF_SOC_MARGIN;
    }

    return limited;
}

/**
 * @brief  Defence #2: reject NaN, infinity and impossible current spikes.
 * @param  p_kf     Filter state, for the last accepted current.
 * @param  current  Raw current reading, A.
 * @return The reading when it is plausible, the last accepted value if not.
 */
static float kf_sanitize_current(KF_State_t *p_kf, float current)
{
    float sane = current;

    if ((!isfinite(current)) || (fabsf(current) > KF_MAX_CURRENT))
    {
        sane = p_kf->i_prev;
    }

    return sane;
}
