#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
sopwith_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cmake -S "$sopwith_root" -B "$sopwith_root/build/tests" \
    -DSOPWITH_TESTS=ON -DSOPWITH_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build "$sopwith_root/build/tests" --parallel 2
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir "$sopwith_root/build/tests" --output-on-failure
