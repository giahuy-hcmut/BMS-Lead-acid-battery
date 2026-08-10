#!/usr/bin/env bash
# Monte-Carlo runs: 30 noise seeds x 3 configurations.
# Output: sim/mc_results.txt (raw) + summary table on stdout.
set -e
cd "$(dirname "$0")/.."

OUT=sim/mc_results.txt
: > "$OUT"

run_config () {   # $1 name  $2 DRIVE_CYCLE  $3 WRONG_INIT  $4 QF  $5 BIAS
  sed -i "s|#define DRIVE_CYCLE  .*|#define DRIVE_CYCLE  $2|; \
          s|#define WRONG_INIT   .*|#define WRONG_INIT   $3|; \
          s|#define Q_TRUE_FACTOR .*|#define Q_TRUE_FACTOR $4f|; \
          s|#define I_BIAS       .*|#define I_BIAS       $5f|" sim/simulate.c
  # SOC_Kalman lives in the firmware tree (Core/), the same single copy the
  # unit tests and the STM32 build use - see run.sh.
  gcc -std=c11 -I ../Core/Inc sim/simulate.c ../Core/Src/SOC_Kalman.c -o sim/simulate -lm
  for s in $(seq 1 30); do
    ./sim/simulate "$s" > /dev/null
    awk -F, -v cfg="$1" -v seed="$s" 'NR>2{n++; c+=($7-$6)^2; h+=($8-$6)^2; k+=($9-$6)^2}
      END{printf "%s %d %.4f %.4f %.4f\n", cfg, seed, sqrt(c/n), sqrt(h/n), sqrt(k/n)}' \
      sim_result.csv >> "$OUT"
  done
}

run_config ece_baseline  0 0 1.0  0.125
run_config ece_realistic 0 1 0.90 0.125
run_config dst_realistic 1 1 0.90 0.125

# restore default config (ECE-15 realistic)
sed -i "s|#define DRIVE_CYCLE  .*|#define DRIVE_CYCLE  0|; \
        s|#define WRONG_INIT   .*|#define WRONG_INIT   1|; \
        s|#define Q_TRUE_FACTOR .*|#define Q_TRUE_FACTOR 0.90f|; \
        s|#define I_BIAS       .*|#define I_BIAS       0.125f|" sim/simulate.c

echo "=== MONTE CARLO SUMMARY (n=30 seeds) — RMSE % mean +/- std ==="
awk '{
  n[$1]++
  for (i=3;i<=5;i++){ s[$1,i]+=$i; q[$1,i]+=$i*$i }
}
END{
  printf "%-14s | %-16s %-16s %-16s\n","config","CC","Hybrid","KF"
  for (c in n){
    line=sprintf("%-14s |",c)
    for (i=3;i<=5;i++){
      m=s[c,i]/n[c]; sd=sqrt(q[c,i]/n[c]-m*m)
      line=line sprintf(" %6.3f +/- %5.3f",m,sd)
    }
    print line
  }
}' "$OUT"
