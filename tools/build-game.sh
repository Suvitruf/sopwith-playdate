#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
sopwith_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# Game builds default to the SDK that can run on the connected OS 2.2 console.
export PLAYDATE_SDK_PATH="${PLAYDATE_SDK_PATH:-$sopwith_root/.tools/PlaydateSDK-2.2.0}"
source "$sopwith_root/tools/env.sh"
sopwith_version="$(tr -d '\r\n' < "$PLAYDATE_SDK_PATH/VERSION.txt")"
sopwith_build="$sopwith_root/build/game-$sopwith_version"
cmake -S "$sopwith_root" -B "$sopwith_build/simulator" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$sopwith_build/simulator" --parallel 2
cmake -S "$sopwith_root" -B "$sopwith_build/device" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$PLAYDATE_SDK_PATH/C_API/buildsupport/arm.cmake"
cmake --build "$sopwith_build/device" --parallel 2
test -s "$sopwith_build/simulator/sopwith.pdx/pdex.so"
test -s "$sopwith_build/device/sopwith.pdx/pdex.bin"
echo "Built Simulator and device bundles in $sopwith_build"
