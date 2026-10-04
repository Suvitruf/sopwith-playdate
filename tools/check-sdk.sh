#!/usr/bin/env bash
# Build an original, minimal C program for both Simulator and hardware.
set -euo pipefail
sopwith_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$sopwith_root/tools/env.sh"
for command in cmake cc arm-none-eabi-gcc arm-none-eabi-objcopy; do
    command -v "$command" >/dev/null || { echo "Missing command: $command" >&2; exit 1; }
done
test -f "$PLAYDATE_SDK_PATH/C_API/pd_api.h"
"$PLAYDATE_SDK_PATH/bin/pdc" --version
arm-none-eabi-gcc --version | head -n 1
sopwith_sdk_version="$(tr -d '\r\n' < "$PLAYDATE_SDK_PATH/VERSION.txt")"
sopwith_check="$sopwith_root/build/sdk-smoke"
if [[ "$sopwith_sdk_version" != 3.1.2 ]]; then
    sopwith_check="$sopwith_check-$sopwith_sdk_version"
fi
mkdir -p "$sopwith_check/project"
cp -R "$sopwith_root/tools/sdk-smoke/." "$sopwith_check/project/"
sopwith_product_options=(-DPRODUCT_DIR="$sopwith_check/output/simulator")
if [[ "$sopwith_sdk_version" == 2.2.0 ]]; then
    # This SDK predates PRODUCT_DIR and uses the same package name for both targets.
    sopwith_product_options=()
fi
cmake -S "$sopwith_check/project" -B "$sopwith_check/simulator" \
    -DCMAKE_BUILD_TYPE=Debug "${sopwith_product_options[@]}"
cmake --build "$sopwith_check/simulator" --parallel 2
if [[ "$sopwith_sdk_version" == 2.2.0 ]]; then
    mkdir -p "$sopwith_check/output/simulator/sdk_check.pdx"
    cp -R "$sopwith_check/project/sdk_check.pdx/." "$sopwith_check/output/simulator/sdk_check.pdx/"
else
    sopwith_product_options=(-DPRODUCT_DIR="$sopwith_check/output/device")
fi
# SDK build support stages native binaries into Source; run builds sequentially.
cmake -S "$sopwith_check/project" -B "$sopwith_check/device" \
    -DCMAKE_BUILD_TYPE=Release "${sopwith_product_options[@]}" \
    -DCMAKE_TOOLCHAIN_FILE="$PLAYDATE_SDK_PATH/C_API/buildsupport/arm.cmake"
cmake --build "$sopwith_check/device" --parallel 2
if [[ "$sopwith_sdk_version" == 2.2.0 ]]; then
    mkdir -p "$sopwith_check/output/device/sdk_check_device.pdx"
    cp -R "$sopwith_check/project/sdk_check.pdx/." "$sopwith_check/output/device/sdk_check_device.pdx/"
fi
test -s "$sopwith_check/output/simulator/sdk_check.pdx/pdex.so"
test -s "$sopwith_check/output/device/sdk_check_device.pdx/pdex.bin"
echo "PASS: Simulator library and ARM device bundle built. This is an SDK check, not the Sopwith game."
