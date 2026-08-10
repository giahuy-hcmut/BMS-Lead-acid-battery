/*
 * test_SOC_Kalman.c
 *
 * Unit tests for the MULTIRATE SOC Kalman filter, organized by the course's
 * 10 error-group checklist (UnitTest sheet). Each test names the technique
 * it applies (BVA, robustness, fault injection, ...).
 *
 * Multirate: Predict() runs on every current sample (fast, 5 ms);
 * Update() runs only when a fresh voltage sample arrives (slow, 50 ms).
 *
 * Run:  bash run.sh        (from the UnitTest folder)
 */

#include "unity.h"
#include "SOC_Kalman.h"
#include <math.h>

#define DT_FAST   0.005f    /* 5 ms current / predict period */

static KF_State_t kf;

void setUp(void)    {}
void tearDown(void) {}

/* =====================================================================
 * GROUP #3 DECISION LOGIC + INITIALIZATION  --  Boundary Value Analysis
 * ===================================================================== */

void test_init_full_battery(void)
{
    SOC_Kalman_Init(&kf, KF_OCV_FULL);                 /* full -> SOC 1.0 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, kf.soc);
}

void test_init_empty_battery(void)
{
    SOC_Kalman_Init(&kf, KF_OCV_EMPTY);                 /* empty -> SOC 0.0 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, kf.soc);
}

void test_init_half_battery(void)
{
    SOC_Kalman_Init(&kf, (KF_OCV_EMPTY + 0.5f * KF_OCV_SLOPE));                /* midpoint -> SOC 0.5 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, kf.soc);
}

void test_init_above_full_clamps_to_one(void)
{
    SOC_Kalman_Init(&kf, 13.0f);                 /* clamp high */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, kf.soc);
}

void test_init_below_empty_clamps_to_zero(void)
{
    SOC_Kalman_Init(&kf, 11.0f);                 /* clamp low */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, kf.soc);
}

/* =====================================================================
 * GROUP #2 CONTROL FLOW  --  stability over many iterations
 * ===================================================================== */

void test_rest_zero_current_holds_soc(void)
{
    /* I = 0 and matching voltage -> SOC must not drift over 1000 cycles */
    SOC_Kalman_Init(&kf, KF_OCV_FULL);
    float soc = 1.0f;
    for (int i = 0; i < 1000; i++) {
        SOC_Kalman_Predict(&kf, 0.0f, DT_FAST);
        soc = SOC_Kalman_Update(&kf, KF_OCV_FULL, 0.0f, 25.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, soc);
}

/* =====================================================================
 * GROUP #1 MATHEMATICAL  --  Back-to-back: PREDICT must match Coulomb
 * ===================================================================== */

void test_predict_matches_coulomb_counting(void)
{
    /* Pure predict (no voltage update): discharge 50 A for 720 s at dt=1 s.
     * charge removed = 50*720 = 36000 A.s = half of 72000 -> SOC 1.0 -> 0.5 */
    SOC_Kalman_Init(&kf, KF_OCV_FULL);
    for (int i = 0; i < 720; i++) {
        SOC_Kalman_Predict(&kf, 50.0f, 1.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, kf.soc);
}

/* =====================================================================
 * GROUP #1/#6 SCALING & CAST  --  SOC*100 must fit uint8 [0..100]
 * ===================================================================== */

void test_soc_always_fits_uint8_percent(void)
{
    SOC_Kalman_Init(&kf, KF_OCV_FULL);
    SOC_Kalman_Predict(&kf, 0.0f, DT_FAST);
    float soc = SOC_Kalman_Update(&kf, KF_OCV_FULL, 0.0f, 25.0f);
    int pct = (int)(soc * 100.0f);
    TEST_ASSERT_TRUE(pct >= 0 && pct <= 100);
}

/* =====================================================================
 * GROUP #8 SAFETY  --  Fault injection: current spike in PREDICT
 * ===================================================================== */

void test_predict_spike_rejected_by_sanity(void)
{
    /* A spike above KF_MAX_CURRENT -> held at the last value (0), so Predict
     * must not integrate it.
     * Derived from the macro, never hardcoded: this used to say 100 A back when
     * the limit was 60 A, and raising the limit to 250 A silently turned that
     * "spike" into a legitimate current the filter was right to accept. */
    SOC_Kalman_Init(&kf, KF_OCV_FULL);
    SOC_Kalman_Predict(&kf, KF_MAX_CURRENT * 2.0f, DT_FAST);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, kf.soc);
}

/* =====================================================================
 * GROUP #8 SAFETY  --  Physics validator (in UPDATE): current lies
 * ===================================================================== */

void test_update_validator_zeros_lying_current(void)
{
    /* Current claims 50 A discharge but fresh voltage shows NO sag (12.6 V).
     * Validator must zero the current in the model, so a single update from
     * a full battery leaves SOC at ~1.0 (no false correction downward). */
    SOC_Kalman_Init(&kf, KF_OCV_FULL);                 /* SOC = 1.0 */
    float soc = SOC_Kalman_Update(&kf, KF_OCV_FULL, 50.0f, 25.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, soc);
}

/* =====================================================================
 * GROUP #8 SAFETY  --  REGRESSION: validator must NOT kill legitimate
 * regen current (I < 0 lifts voltage above OCV — that is valid physics,
 * not a lying sensor). Bug found via SOC spikes at every ECE-15 braking.
 * ===================================================================== */

void test_update_validator_accepts_regen(void)
{
    /* Battery at 50%, regen -15 A: terminal voltage rises above OCV by
     * |I|*R0. A correct validator accepts this; the old sag-only logic
     * zeroed the current and caused an upward SOC spike. */
    float ocv50   = KF_OCV_EMPTY + 0.5f * KF_OCV_SLOPE;
    SOC_Kalman_Init(&kf, ocv50);                  /* SOC = 0.5 */
    float v_regen = ocv50 + 15.0f * KF_R0_NOM;    /* charging lifts V */
    float soc = SOC_Kalman_Update(&kf, v_regen, -15.0f, 25.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.5f, soc);   /* no spike */
}

/* =====================================================================
 * GROUP #3 DECISION LOGIC  --  Innovation gating rejects outlier voltage
 * ===================================================================== */

void test_update_gating_rejects_voltage_outlier(void)
{
    /* A wild 5.0 V reading (vs expected ~12.6) must be gated out. */
    SOC_Kalman_Init(&kf, KF_OCV_FULL);
    float soc = SOC_Kalman_Update(&kf, 5.0f, 0.0f, 25.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, soc);
}

/* =====================================================================
 * GROUP #4/#8 CORRECTION  --  Kalman converges from a wrong initial SOC
 * ===================================================================== */

void test_converges_from_wrong_init(void)
{
    /* Init wrongly at empty but battery is actually full: feed the true
     * resting voltage 12.6 with I=0; filter must climb toward full. */
    SOC_Kalman_Init(&kf, KF_OCV_EMPTY);                 /* wrong: says empty */
    float soc = 0.0f;
    for (int i = 0; i < 5000; i++) {
        SOC_Kalman_Predict(&kf, 0.0f, DT_FAST);
        soc = SOC_Kalman_Update(&kf, KF_OCV_FULL, 0.0f, 25.0f);
    }
    TEST_ASSERT_TRUE(soc > 0.9f);
}

/* =====================================================================
 * GROUP #8/#9 ROBUSTNESS  --  garbage inputs must not crash / NaN-out
 * ===================================================================== */

void test_predict_nan_current_is_safe(void)
{
    SOC_Kalman_Init(&kf, KF_OCV_FULL);
    SOC_Kalman_Predict(&kf, NAN, DT_FAST);
    TEST_ASSERT_FLOAT_IS_DETERMINATE(kf.soc);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, kf.soc);   /* rejected */
}

void test_update_nan_voltage_is_safe(void)
{
    SOC_Kalman_Init(&kf, KF_OCV_FULL);
    SOC_Kalman_Predict(&kf, 0.0f, DT_FAST);
    float soc = SOC_Kalman_Update(&kf, NAN, 0.0f, 25.0f);   /* no update */
    TEST_ASSERT_FLOAT_IS_DETERMINATE(soc);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, soc);
}

void test_predict_huge_current_is_safe(void)
{
    SOC_Kalman_Init(&kf, KF_OCV_FULL);
    SOC_Kalman_Predict(&kf, 1.0e9f, DT_FAST);
    TEST_ASSERT_FLOAT_IS_DETERMINATE(kf.soc);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, kf.soc);
}

/* =====================================================================
 * GROUP #8 STRESS  --  realistic multirate fuzz: predict 5ms, update 50ms
 * ===================================================================== */

void test_multirate_long_run_stays_in_range(void)
{
    SOC_Kalman_Init(&kf, KF_OCV_FULL);
    for (int i = 0; i < 10000; i++) {
        float I = (float)((i % 80) - 20);          /* -20 .. +59 A */
        SOC_Kalman_Predict(&kf, I, DT_FAST);       /* every 5 ms */
        if (i % 10 == 0) {                         /* every 50 ms */
            SOC_Kalman_Update(&kf, 12.0f, I, 25.0f);
        }
        TEST_ASSERT_FLOAT_IS_DETERMINATE(kf.soc);
        /* REPORTED SOC must stay in [0,1]; internal state may use the
         * small soft-clamp margin. */
        float out = SOC_Kalman_GetSOC(&kf);
        TEST_ASSERT_TRUE(out >= 0.0f && out <= 1.0f);
        TEST_ASSERT_TRUE(kf.soc >= -KF_SOC_MARGIN - 0.001f &&
                         kf.soc <=  1.0f + KF_SOC_MARGIN + 0.001f);
    }
}

/* =====================================================================
 * GROUP #3 BOUNDARY  --  soft clamp: internal state may overshoot within
 * the margin, but the REPORTED SOC never leaves [0,1].
 * ===================================================================== */

void test_soft_clamp_reported_soc_bounded(void)
{
    /* Battery full; feed voltage slightly ABOVE full OCV (charger still
     * connected). Internal state may rise into the margin; the official
     * output must stay exactly at 100%. */
    SOC_Kalman_Init(&kf, KF_OCV_FULL);
    for (int i = 0; i < 200; i++) {
        SOC_Kalman_Predict(&kf, 0.0f, DT_FAST);
        SOC_Kalman_Update(&kf, KF_OCV_FULL + 0.05f, 0.0f, 25.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, SOC_Kalman_GetSOC(&kf));
    TEST_ASSERT_TRUE(kf.soc <= 1.0f + KF_SOC_MARGIN + 0.001f);
}

void test_soft_clamp_reported_soc_bounded_at_floor(void)
{
    /* Battery empty; voltage keeps reading slightly BELOW empty OCV.
     * Internal state may dip into the negative margin; the official
     * output must stay exactly at 0%. */
    SOC_Kalman_Init(&kf, KF_OCV_EMPTY);
    for (int i = 0; i < 200; i++) {
        SOC_Kalman_Predict(&kf, 0.0f, DT_FAST);
        SOC_Kalman_Update(&kf, KF_OCV_EMPTY - 0.05f, 0.0f, 25.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, SOC_Kalman_GetSOC(&kf));
    TEST_ASSERT_TRUE(kf.soc >= -KF_SOC_MARGIN - 0.001f);
}

/* =====================================================================
 * GROUP #8 REALISTIC SCENARIO  --  one full ECE-15 urban cycle
 * (UNECE Regulation 83: 18 segments, 195 s, max 50 km/h) driven through
 * a simplified vehicle model (Murata: 535 kg) and a plant WITH the RC
 * polarization dynamics. Covers acceleration, cruise, regen braking
 * (negative current) and idle. The filter must track the true SOC.
 * ===================================================================== */

void test_ev_drive_cycle_tracks_truth(void)
{
    /* ECE-15 segments: start speed, end speed (km/h), duration (s) */
    static const struct { float v0, v1, dur; } seg[] = {
        {  0,  0, 11 }, {  0, 15,  4 }, { 15, 15,  8 }, { 15,  0,  5 },
        {  0,  0, 21 }, {  0, 15,  6 }, { 15, 32,  6 }, { 32, 32, 24 },
        { 32,  0, 11 }, {  0,  0, 21 }, {  0, 15,  6 }, { 15, 35, 11 },
        { 35, 50,  9 }, { 50, 50, 12 }, { 50, 35,  8 }, { 35, 35, 15 },
        { 35,  0, 10 }, {  0,  0,  7 },
    };
    const int n_seg = (int)(sizeof(seg) / sizeof(seg[0]));
    const float dt = DT_FAST;

    /* Start below full so ceiling effects cannot mask tracking. */
    float true_soc = 0.90f;
    float vrc_true = 0.0f;
    SOC_Kalman_Init(&kf, KF_OCV_EMPTY + true_soc * KF_OCV_SLOPE);

    float a_rc = 1.0f - dt / (KF_R1_NOM * KF_C1_NOM);
    float b_rc = dt / KF_C1_NOM;

    int n = 0;
    for (int s = 0; s < n_seg; s++) {
        int steps = (int)(seg[s].dur / dt);
        float a_ms2 = ((seg[s].v1 - seg[s].v0) / 3.6f) / seg[s].dur;
        for (int k = 0; k < steps; k++, n++) {
            /* speed within segment + simplified Murata road-load model */
            float v = (seg[s].v0 +
                       (seg[s].v1 - seg[s].v0) * ((float)k / steps)) / 3.6f;
            float F = 535.0f * a_ms2 +                       /* inertia   */
                      0.5f * 1.2f * 0.4f * 1.93f * v * v;    /* aero drag */
            if (v > 0.05f) F += 0.015f * 535.0f * 9.81f;     /* rolling   */
            float P = F * v;
            float I = ((F >= 0.0f) ? P / 0.85f : P * 0.6f) / 60.0f;
            if (I >  50.0f) I =  50.0f;
            if (I < -50.0f) I = -50.0f;

            /* plant WITH polarization dynamics (same physics the filter
             * models — this is a functional tracking test, robustness to
             * parameter mismatch is covered by the offline simulation) */
            float v_plant = KF_OCV_EMPTY + KF_OCV_SLOPE * true_soc
                            - I * KF_R0_NOM - vrc_true;
            vrc_true = a_rc * vrc_true + b_rc * I;
            true_soc -= I * dt / KF_Q_RATED;

            SOC_Kalman_Predict(&kf, I, dt);          /* every 5 ms  */
            if (n % 10 == 0) {                        /* every 50 ms */
                SOC_Kalman_Update(&kf, v_plant, I, 25.0f);
            }
        }
    }

    /* Filter must track the true SOC through the standard cycle... */
    TEST_ASSERT_FLOAT_WITHIN(0.02f, true_soc, SOC_Kalman_GetSOC(&kf));
    /* ...and register a net discharge over the cycle. */
    TEST_ASSERT_TRUE(SOC_Kalman_GetSOC(&kf) < 0.90f);
}

/* ===================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* #3 Init / BVA */
    RUN_TEST(test_init_full_battery);
    RUN_TEST(test_init_empty_battery);
    RUN_TEST(test_init_half_battery);
    RUN_TEST(test_init_above_full_clamps_to_one);
    RUN_TEST(test_init_below_empty_clamps_to_zero);
    /* #2 Control flow */
    RUN_TEST(test_rest_zero_current_holds_soc);
    /* #1 Math / back-to-back */
    RUN_TEST(test_predict_matches_coulomb_counting);
    /* #1/#6 Scaling & cast */
    RUN_TEST(test_soc_always_fits_uint8_percent);
    /* #8 Fault injection */
    RUN_TEST(test_predict_spike_rejected_by_sanity);
    RUN_TEST(test_update_validator_zeros_lying_current);
    RUN_TEST(test_update_validator_accepts_regen);
    /* #3 Gating */
    RUN_TEST(test_update_gating_rejects_voltage_outlier);
    /* #4 Correction */
    RUN_TEST(test_converges_from_wrong_init);
    /* #8/#9 Robustness */
    RUN_TEST(test_predict_nan_current_is_safe);
    RUN_TEST(test_update_nan_voltage_is_safe);
    RUN_TEST(test_predict_huge_current_is_safe);
    /* #8 Stress */
    RUN_TEST(test_multirate_long_run_stays_in_range);
    RUN_TEST(test_soft_clamp_reported_soc_bounded);
    RUN_TEST(test_soft_clamp_reported_soc_bounded_at_floor);
    /* #8 Realistic EV drive cycle */
    RUN_TEST(test_ev_drive_cycle_tracks_truth);

    return UNITY_END();
}
