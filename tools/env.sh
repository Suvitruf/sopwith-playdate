#!/usr/bin/env bash
# Source this file from Bash: source tools/env.sh
_sopwith_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export PLAYDATE_SDK_PATH="${PLAYDATE_SDK_PATH:-$_sopwith_root/.tools/PlaydateSDK-3.1.2}"
export PATH="$PLAYDATE_SDK_PATH/bin:$_sopwith_root/.tools/arm-toolchain/usr/bin:$PATH"
unset _sopwith_root
