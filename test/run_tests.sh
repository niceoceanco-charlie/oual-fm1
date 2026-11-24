#!/bin/sh
# Builds with warnings on, plus an UBSan (AddressSanitizer hangs at start-up on the dev Mac, so it is not used) run. From the repo root: ./test/run_tests.sh
set -e
cd "$(dirname "$0")/.."
cc -O2 -Wall -Wextra -o /tmp/oual_test test/test_engine.c core/oual_engine.c -lm
/tmp/oual_test
cc -g -O1 -fsanitize=undefined -fno-sanitize-recover=undefined -o /tmp/oual_test_san test/test_engine.c core/oual_engine.c -lm
/tmp/oual_test_san | tail -1
