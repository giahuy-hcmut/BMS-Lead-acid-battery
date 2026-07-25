/*
 * SOC_Kalman.c
 *
 * 2-state Kalman filter SOC estimator, MULTIRATE design:
 *   - SOC_Kalman_Predict() runs at the fast current rate (5 ms).
 *   - SOC_Kalman_Update()  runs only when a fresh voltage sample exists (50 ms).
 * This avoids ever correcting with a stale voltage reading.
 *
 * All matrix math is expanded to scalars (2x2) so no linear-algebra
 * library is needed and it runs in ~20 us on an STM32F103 (no FPU).
 */

#include "SOC_Kalman.h"
#include <math.h>

/* Open-circuit voltage from SOC (the straight line OCV = 11.5 + 1.1*SOC). */
static float ocv_from_soc(float soc)
{
    return KF_OCV_EMPTY + KF_OCV_SLOPE * soc;
}

/* Soft clamp for the INTERNAL state: allow a small overshoot beyond [0,1]
 * so measurement noise around a full/empty battery stays symmetric.
 * A hard clamp here would bias the estimate upward at 100% (noise pushing
 * up is cut, noise pushing down is kept) making the SOC stick at full. */
static float clamp_soc(float soc)
{
    if (soc > 1.0f + KF_SOC_MARGIN) return 1.0f + KF_SOC_MARGIN;
    if (soc < 0.0f - KF_SOC_MARGIN) return 0.0f - KF_SOC_MARGIN;
    return soc;
}

float SOC_Kalman_GetSOC(const KF_State_t *kf)
{
    if (kf->soc > 1.0f) return 1.0f;
    if (kf->soc < 0.0f) return 0.0f;
    return kf->soc;
}

/* Defense #2: reject NaN / inf / physically impossible current spikes,
 * holding the last accepted value instead. Returns a sane current. */
static float sanitize_current(KF_State_t *kf, float current)
{
    if (!isfinite(current) || fabsf(current) > KF_MAX_CURRENT) {
        return kf->i_prev;
    }
    return current;
}

void SOC_Kalman_Init(KF_State_t *kf, float v_measured)
{
    /* At boot the battery rests (I ~ 0), so terminal voltage ~ OCV.
     * Invert OCV(SOC) -> SOC = (V - OCV_EMPTY) / OCV_SLOPE.
     * HARD clamp here: the soft margin exists only to keep run-time noise
     * symmetric; a one-shot boot estimate must start inside [0,1]. */
    float soc0 = (v_measured - KF_OCV_EMPTY) / KF_OCV_SLOPE;
    if (soc0 > 1.0f) soc0 = 1.0f;
    if (soc0 < 0.0f) soc0 = 0.0f;

    kf->soc  = soc0;
    kf->v_rc = 0.0f;

    /* Start "unsure" so the filter trusts measurements early, then converges. */
    kf->P[0][0] = 1.0f; kf->P[0][1] = 0.0f;
    kf->P[1][0] = 0.0f; kf->P[1][1] = 1.0f;

    kf->i_prev = 0.0f;
}

void SOC_Kalman_Predict(KF_State_t *kf, float current, float dt)
{
    current = sanitize_current(kf, current);   /* defense #2 */
    kf->i_prev = current;

    /* State prediction:
     *   SOC  drops by the charge pulled out this step
     *   V_RC relaxes toward its steady value via the RC time constant */
    float a = 1.0f - dt / (KF_R1_NOM * KF_C1_NOM);   /* RC decay factor */
    float b = dt / KF_C1_NOM;

    float soc_p = kf->soc  - (dt / KF_Q_RATED) * current;
    float vrc_p = a * kf->v_rc + b * current;

    /* Covariance prediction: P = A*P*A' + Q, with A = [[1,0],[0,a]] */
    float p00 = kf->P[0][0] + KF_Q_SOC;
    float p01 = a * kf->P[0][1];
    float p10 = a * kf->P[1][0];
    float p11 = a * a * kf->P[1][1] + KF_Q_VRC;

    kf->soc  = clamp_soc(soc_p);
    kf->v_rc = vrc_p;
    kf->P[0][0] = p00; kf->P[0][1] = p01;
    kf->P[1][0] = p10; kf->P[1][1] = p11;
}

float SOC_Kalman_Update(KF_State_t *kf, float v_measured,
                        float current, float temp)
{
    /* No update without a valid measurement (defense #9). */
    if (!isfinite(v_measured)) {
        return SOC_Kalman_GetSOC(kf);
    }

    current = sanitize_current(kf, current);   /* defense #2 */

    /* Temperature-adjusted ohmic resistance (colder -> higher R0). */
    float R0 = KF_R0_NOM * (1.0f + KF_ALPHA_R0 * (KF_TEMP_REF - temp));

    /* Defense #4: physics validator — DISCHARGE ONLY.
     * A large discharge current MUST sag the terminal voltage. If a big
     * positive current is reported but the voltage barely sagged, the sensor
     * is lying (stuck/offset) -> zero it in the measurement model.
     * Deliberately NOT applied to regen (I < 0): with the polarization
     * voltage V_RC still decaying after a load phase, an OCV-based sag test
     * misreads legitimate charging as a fault (caused SOC spikes at every
     * ECE-15 braking). Corrupt regen readings are still caught by the
     * innovation gate (#3) and the sanity limit (#2). */
    if (current > KF_VALID_I_MIN) {
        float ocv_est      = ocv_from_soc(kf->soc);
        float expected_sag = current * R0;
        float actual_sag   = ocv_est - v_measured;    /* >0 when discharging */
        if (actual_sag < KF_VALID_RATIO * expected_sag) {
            current = 0.0f;
        }
    }
    kf->i_prev = current;

    /* Predicted terminal voltage from the (already predicted) state. */
    float v_pred     = ocv_from_soc(kf->soc) - current * R0 - kf->v_rc;
    float innovation = v_measured - v_pred;

    /* Measurement Jacobian H = d(v_pred)/d(state) = [OCV_SLOPE, -1] */
    const float h0 = KF_OCV_SLOPE;
    const float h1 = -1.0f;

    /* Innovation covariance S = H*P*H' + R  (scalar). R>0 so S>0 always. */
    float p00 = kf->P[0][0], p01 = kf->P[0][1];
    float p10 = kf->P[1][0], p11 = kf->P[1][1];
    float ph0 = p00 * h0 + p01 * h1;   /* (P*H')[0] */
    float ph1 = p10 * h0 + p11 * h1;   /* (P*H')[1] */
    float S   = h0 * ph0 + h1 * ph1 + KF_R_MEAS;

    /* Defense #3: innovation gating. If the mismatch is far larger than
     * statistically expected, the measurement is corrupt -> skip correction. */
    if (fabsf(innovation) > KF_GATE_SIGMA * sqrtf(S)) {
        return SOC_Kalman_GetSOC(kf);
    }

    /* Kalman gain K = P*H' / S  (2x1) */
    float k0 = ph0 / S;
    float k1 = ph1 / S;

    /* Correct the state by the gained innovation. */
    float soc_new = kf->soc  + k0 * innovation;
    float vrc_new = kf->v_rc + k1 * innovation;

    /* Covariance update: P = (I - K*H) * P */
    float n00 = (1.0f - k0 * h0) * p00 + (-k0 * h1) * p10;
    float n01 = (1.0f - k0 * h0) * p01 + (-k0 * h1) * p11;
    float n10 = (-k1 * h0) * p00 + (1.0f - k1 * h1) * p10;
    float n11 = (-k1 * h0) * p01 + (1.0f - k1 * h1) * p11;

    kf->soc  = clamp_soc(soc_new);
    kf->v_rc = vrc_new;
    kf->P[0][0] = n00; kf->P[0][1] = n01;
    kf->P[1][0] = n10; kf->P[1][1] = n11;

    return SOC_Kalman_GetSOC(kf);
}
