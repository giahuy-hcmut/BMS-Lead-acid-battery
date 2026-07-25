/*
 * simulate.c  --  GĐ3 offline evaluation (PC only, never flashed).
 *
 * Pipeline:
 *   ECE-15 urban drive cycle (UNECE R83)  ->  vehicle dynamics  ->  current
 *   -> virtual battery (true SOC + terminal voltage)  ->  3 estimators:
 *        cc     : plain Coulomb counting
 *        hybrid : Coulomb + OCV recalibration at rest
 *        kf     : the REAL SOC_Kalman module under test
 *
 * ECE-15 speed profile is the authoritative 18-segment definition
 * (source: UNECE R83; data via github.com/dabo248/nedc, udc.csv),
 * max 50 km/h -> suitable for a light 60V electric vehicle.
 *
 * Output sim_result.csv:
 *   time, speed_kmh, current, true_soc, cc_soc, hybrid_soc, kf_soc   (SOC %)
 *
 * Build & run (from UnitTest/):
 *   gcc -Wall -std=c11 -I src sim/simulate.c src/SOC_Kalman.c -o sim/simulate -lm
 *   ./sim/simulate
 */

#include "SOC_Kalman.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ---- scenario select ---- */
#define WRONG_INIT   1
#define INIT_SOC     ((WRONG_INIT) ? 0.75f : 1.0f)

/* ---- drive cycle select ----
 * 0 = ECE-15 urban VEHICLE cycle (UNECE R83): speed -> vehicle dynamics -> I
 * 1 = DST BATTERY cycle (USABC/INL Battery Test Manual for EVs, Table 3):
 *     360 s, 20 constant-power steps in % of peak power, applied directly. */
#define DRIVE_CYCLE  0

/* ---- simulation timing ---- */
#define DT_FAST      0.005f     /* 5 ms  -> current + predict rate */
#define VOLT_EVERY   10         /* voltage sample every 10 steps = 50 ms */
#define T_END        3600.0f    /* total simulated time (s) = 1 h (~18 ECE-15 cycles) */
#define LOG_EVERY    200        /* write one CSV row per 200 steps = 1 s */

/* ---- 60V light electric vehicle parameters ---- */
#define VEH_M        535.0f      /* total laden mass, 1 driver, from Murata layout drawing (kg) */
#define VEH_CRR      0.015f      /* rolling resistance coefficient */
#define VEH_CD       0.4f        /* aerodynamic drag coefficient */
#define VEH_AREA     1.93f      /* frontal area = 0.85 x 1.414 x 1.602 m (Murata drawing) */
#define AIR_RHO      1.2f        /* air density (kg/m^3) */
#define GRAV         9.81f
#define ETA_DRIVE    0.85f       /* motor + controller efficiency (traction) */
#define ETA_REGEN    0.6f        /* energy fraction recovered when braking */
#define V_PACK       60.0f       /* pack voltage (V) */
#define I_CAP        50.0f       /* controller current limit (A) */

/* ---- TRUE plant parameters (a bit off from the filter's assumed values) ---- */
#define R0_TRUE      0.01377f   /* CSB 13.5mOhm +2% (HPPC-level mismatch) */
#define R1_TRUE      0.01377f   /* R1 nom +2% (HPPC-level mismatch) */
#define C1_TRUE      1813.0f    /* C1 nom -2% */

/* Real (aged) capacity < nominal KF_Q_RATED that the estimators assume.
 * A CC that integrates against the nominal capacity therefore drifts;
 * the Kalman voltage update corrects it, CC cannot. */
#define Q_TRUE_FACTOR 0.90f
#define Q_TRUE        (KF_Q_RATED * Q_TRUE_FACTOR)

/* ---- sensor imperfections ---- */
#define I_BIAS       0.125f
#define I_NOISE      0.15f       /* current noise amplitude (A) */
#define V_NOISE      0.02f       /* voltage noise: good-hardware level, ADC 12-bit + 100-avg + margin */

/* ---- hybrid estimator: rest detection + OCV pull strength ---- */
#define REST_THRESH  1.5f
#define OCV_PULL     0.10f

/* ================= ECE-15 urban drive cycle (18 segments) ================= */
/* start & end speed in km/h, duration in s. Total = 195 s, max 50 km/h. */
typedef struct { float v0, v1, dur; } Seg;
static const Seg ECE15[] = {
    {  0,  0, 11 }, {  0, 15,  4 }, { 15, 15,  8 }, { 15,  0,  5 },
    {  0,  0, 21 }, {  0, 15,  6 }, { 15, 32,  6 }, { 32, 32, 24 },
    { 32,  0, 11 }, {  0,  0, 21 }, {  0, 15,  6 }, { 15, 35, 11 },
    { 35, 50,  9 }, { 50, 50, 12 }, { 50, 35,  8 }, { 35, 35, 15 },
    { 35,  0, 10 }, {  0,  0,  7 },
};
#define ECE15_N   ((int)(sizeof(ECE15) / sizeof(ECE15[0])))
#define ECE15_LEN 195.0f       /* one cycle length (s) */

/* Speed (m/s) and acceleration (m/s^2) at time t, cycle repeated. */
static void ece15_state(float t, float *v_ms, float *a_ms2)
{
    float ct = fmodf(t, ECE15_LEN);
    float acc = 0.0f;
    for (int i = 0; i < ECE15_N; i++) {
        if (ct < acc + ECE15[i].dur) {
            float local = ct - acc;
            float v_kmh = ECE15[i].v0 +
                          (ECE15[i].v1 - ECE15[i].v0) * (local / ECE15[i].dur);
            *v_ms  = v_kmh / 3.6f;
            *a_ms2 = ((ECE15[i].v1 - ECE15[i].v0) / 3.6f) / ECE15[i].dur;
            return;
        }
        acc += ECE15[i].dur;
    }
    *v_ms = 0.0f; *a_ms2 = 0.0f;
}

/* Pack current (A) demanded by the vehicle at speed v, accel a.
 * >0 = discharge (driving), <0 = regen charge (braking). */
static float vehicle_current(float v, float a)
{
    float f_roll  = (v > 0.05f) ? VEH_CRR * VEH_M * GRAV : 0.0f;
    float f_drag  = 0.5f * AIR_RHO * VEH_CD * VEH_AREA * v * v;
    float f_acc   = VEH_M * a;
    float F       = f_roll + f_drag + f_acc;        /* total tractive force */
    float p_wheel = F * v;                          /* mechanical power (W) */

    float p_elec = (F >= 0.0f) ? p_wheel / ETA_DRIVE   /* traction: losses add */
                               : p_wheel * ETA_REGEN;  /* regen: recover part  */
    float I = p_elec / V_PACK;

    if (I >  I_CAP) I =  I_CAP;      /* controller current limit */
    if (I < -I_CAP) I = -I_CAP;
    return I;
}

/* ================= DST battery cycle (USABC/INL, Table 3) =================
 * 20 constant-power steps, 360 s total. Sign convention here matches the
 * manual: % > 0 = DISCHARGE, % < 0 = charge (regen). Peak power scaled to
 * this vehicle's controller limit: P_peak = 60 V x 50 A = 3 kW. */
typedef struct { float dur, pct; } DstStep;
static const DstStep DST[] = {
    { 16,   0.0f }, { 28,  12.5f }, { 12,  25.0f }, {  8, -12.5f },
    { 16,   0.0f }, { 24,  12.5f }, { 12,  25.0f }, {  8, -12.5f },
    { 16,   0.0f }, { 24,  12.5f }, { 12,  25.0f }, {  8, -12.5f },
    { 16,   0.0f }, { 36,  12.5f }, {  8, 100.0f }, { 24,  62.5f },
    {  8, -25.0f }, { 32,  25.0f }, {  8, -42.9f }, { 44,   0.0f },
};
#define DST_N    ((int)(sizeof(DST) / sizeof(DST[0])))
#define DST_LEN  360.0f
#define DST_PEAK_W (V_PACK * I_CAP)     /* 3 kW peak for this vehicle */

/* Pack current (A) demanded by the DST profile at time t, cycle repeated. */
static float dst_current(float t)
{
    float ct = fmodf(t, DST_LEN);
    float acc = 0.0f;
    for (int i = 0; i < DST_N; i++) {
        if (ct < acc + DST[i].dur) {
            return (DST[i].pct / 100.0f) * DST_PEAK_W / V_PACK;
        }
        acc += DST[i].dur;
    }
    return 0.0f;
}

static float noise(float amp)
{
    return amp * (2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f);
}

static float ocv(float soc)
{
    return KF_OCV_EMPTY + KF_OCV_SLOPE * soc;
}

static float soc_from_ocv(float v)
{
    return (v - KF_OCV_EMPTY) / KF_OCV_SLOPE;
}

static float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

int main(int argc, char **argv)
{
    /* Noise seed from argv[1] for Monte-Carlo runs; default keeps the
     * single-run results reproducible. */
    unsigned seed = (argc > 1) ? (unsigned)atoi(argv[1]) : 12345u;
    srand(seed);

    FILE *f = fopen("sim_result.csv", "w");
    if (!f) { perror("fopen"); return 1; }
    fprintf(f, "time,speed_kmh,i_true,i_meas,v_meas,true_soc,cc_soc,hybrid_soc,kf_soc,v_pred\n");

    /* Plant state (per 12V battery; series pack -> same current everywhere) */
    float soc_true = 1.0f;
    float vrc_true = 0.0f;

    /* Estimators */
    float soc_cc     = INIT_SOC;
    float soc_hybrid = INIT_SOC;
    KF_State_t kf;
    SOC_Kalman_Init(&kf, ocv(1.0f));
#if WRONG_INIT
    kf.soc = INIT_SOC;
#endif

    float a_true = 1.0f - DT_FAST / (R1_TRUE * C1_TRUE);
    float b_true = DT_FAST / C1_TRUE;

    int steps = (int)(T_END / DT_FAST);
    for (int k = 0; k <= steps; k++) {
        float t = k * DT_FAST;

        /* ---- drive cycle -> pack current ---- */
#if DRIVE_CYCLE == 0
        float v_ms, a_ms2;
        ece15_state(t, &v_ms, &a_ms2);
        float i_true = vehicle_current(v_ms, a_ms2);
#else
        float v_ms = 0.0f;                 /* battery cycle: no vehicle speed */
        float i_true = dst_current(t);
#endif

        /* ---- true plant ---- */
        float v_bat = ocv(soc_true) - i_true * R0_TRUE - vrc_true;
        vrc_true = a_true * vrc_true + b_true * i_true;
        soc_true = clampf(soc_true - i_true * DT_FAST / Q_TRUE, 0.0f, 1.0f);

        /* ---- simulated sensors ---- */
        float i_meas = i_true + I_BIAS + noise(I_NOISE);
        float v_meas = v_bat + noise(V_NOISE);

        /* ---- (1) Coulomb counting ---- */
        soc_cc = clampf(soc_cc - i_meas * DT_FAST / KF_Q_RATED, 0.0f, 1.0f);

        /* ---- (2) hybrid: Coulomb + OCV recalibration at rest ---- */
        soc_hybrid = clampf(soc_hybrid - i_meas * DT_FAST / KF_Q_RATED, 0.0f, 1.0f);
        if (k % VOLT_EVERY == 0 && fabsf(i_meas) < REST_THRESH) {
            float ocv_soc = clampf(soc_from_ocv(v_meas), 0.0f, 1.0f);
            soc_hybrid += OCV_PULL * (ocv_soc - soc_hybrid);
        }

        /* ---- (3) Kalman (real module under test) ---- */
        SOC_Kalman_Predict(&kf, i_meas, DT_FAST);
        if (k % VOLT_EVERY == 0) {
            SOC_Kalman_Update(&kf, v_meas, i_meas, 25.0f);
        }

        /* ---- log ---- */
        if (k % LOG_EVERY == 0) {
            /* filter's own terminal-voltage prediction (for the voltage-
             * error metric, comparable to the reference thesis' <0.5%) */
            float v_pred_log = KF_OCV_EMPTY + KF_OCV_SLOPE * kf.soc
                               - i_meas * KF_R0_NOM - kf.v_rc;
            fprintf(f, "%.3f,%.2f,%.3f,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                    t, v_ms * 3.6f, i_true, i_meas, v_meas,
                    soc_true * 100.0f, soc_cc * 100.0f,
                    soc_hybrid * 100.0f, kf.soc * 100.0f, v_pred_log);
        }
    }

    fclose(f);
    printf("Done (WRONG_INIT=%d). ECE-15 cycle, %d rows.\n",
           WRONG_INIT, steps / LOG_EVERY + 1);
    return 0;
}
