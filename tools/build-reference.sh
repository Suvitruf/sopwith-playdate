#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
sopwith_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$sopwith_root"
sopwith_revision="$(python3 -c 'import json; print(json.load(open("tools/dependencies.json"))["upstream"]["revision"])')"
sopwith_src="$sopwith_root/.cache/research/sdl-sopwith-$sopwith_revision/src"
sopwith_out="$sopwith_root/build/reference"
test -f "$sopwith_src/swmain.c" || { echo 'Run bootstrap.py --fetch-upstream first' >&2; exit 1; }
mkdir -p "$sopwith_out"
cat >"$sopwith_out/config.h" <<'CONFIG'
#define HAVE_STDBOOL_H 1
#define HAVE_ISATTY 1
#define PACKAGE_NAME "sopwith-reference"
#define PACKAGE_STRING "SDL Sopwith 2.9.0"
#define PACKAGE_VERSION "2.9.0"
#define HISCORES_PATH ""
CONFIG
if command -v sdl2-config >/dev/null; then
    read -ra sopwith_cflags <<<"$(sdl2-config --cflags)"
    read -ra sopwith_libs <<<"$(sdl2-config --libs)"
else
    sopwith_cflags=(-I"$sopwith_root/.tools/desktop-libs/usr/include/SDL2"
        -I"$sopwith_root/.tools/desktop-libs/usr/include/x86_64-linux-gnu")
    sopwith_libs=(-l:libSDL2-2.0.so.0)
fi
sopwith_common=("$sopwith_src"/*.c "$sopwith_src/sdl/controller.c" "$sopwith_src/sdl/video.c"
    "$sopwith_src/sdl/timer.c" "$sopwith_src/sdl/pcsound.c")
cc -g -O0 -I"$sopwith_out" -I"$sopwith_src" "${sopwith_cflags[@]}" \
    "${sopwith_common[@]}" "$sopwith_src/sdl/main.c" "${sopwith_libs[@]}" -lm -o "$sopwith_out/sopwith"
cc -g -O0 -I"$sopwith_out" -I"$sopwith_src" -Itests "${sopwith_cflags[@]}" \
    "${sopwith_common[@]}" tests/reference_trace.c "${sopwith_libs[@]}" -lm -o "$sopwith_out/trace"
# Keep desktop preferences and scores confined to ignored build output.
XDG_DATA_HOME="$sopwith_out/data" XDG_CONFIG_HOME="$sopwith_out/config" \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_RENDER_DRIVER=software \
    "$sopwith_out/trace" >"$sopwith_out/trace.txt"
echo "Built desktop game and 300-tick reference trace in $sopwith_out"
