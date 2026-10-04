#!/usr/bin/env bash
set -euo pipefail
sopwith_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$sopwith_root/tools/env.sh"
sopwith_libraries="$sopwith_root/.tools/simulator-libs/usr/lib/x86_64-linux-gnu"
if [[ -d "$sopwith_libraries" ]]; then
    export LD_LIBRARY_PATH="$sopwith_libraries${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
exec "$PLAYDATE_SDK_PATH/bin/PlaydateSimulator" \
    "${1:-$sopwith_root/build/sdk-smoke/output/simulator/sdk_check.pdx}"
