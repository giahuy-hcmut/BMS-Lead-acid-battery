#!/usr/bin/env bash
# Build + run unit tests. Chay trong Git Bash.
#   ./run.sh        -> build + chay test
#   ./run.sh cov    -> build + test + bao cao coverage (gcov) cho SOC_Kalman.c
set -e
cd "$(dirname "$0")"

# SOC_Kalman lives in the firmware tree (Core/) - there is exactly ONE copy,
# so these tests exercise the very file that gets flashed onto the STM32.
# Keeping a second copy under UnitTest/src would let the two drift apart the
# first time a filter parameter is retuned.
INC="-I unity -I ../Core/Inc"
CFLAGS="-Wall -Wextra -std=c11"
TEST="test/test_SOC_Kalman.c"
MODULE="../Core/Src/SOC_Kalman.c"

mkdir -p build

if [ "$1" = "cov" ]; then
    rm -f build/*.gcno build/*.gcda *.gcov
    # Compile each source separately with --coverage so gcov can find the
    # instrumentation notes (.gcno) next to each object in build/.
    gcc $CFLAGS --coverage $INC -c "$MODULE" -o build/SOC_Kalman.o
    gcc $CFLAGS --coverage $INC -c "$TEST"   -o build/test.o
    gcc $CFLAGS --coverage $INC -c unity/unity.c -o build/unity.o
    gcc --coverage build/SOC_Kalman.o build/test.o build/unity.o -o build/test_runner -lm
    ./build/test_runner
    echo ""
    echo "--- COVERAGE: SOC_Kalman.c ---"
    gcov -b -o build "$MODULE"
else
    gcc $CFLAGS $INC "$TEST" "$MODULE" unity/unity.c -o build/test_runner -lm
    ./build/test_runner
fi
