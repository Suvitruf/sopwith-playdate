# Development setup

The repository builds a playable prototype for Linux Simulator and ARM hardware. See [TESTING.md](TESTING.md#current-verification-status) for completed checks and remaining hardware acceptance.

## Build the game

From the repository root on Ubuntu 24.04 x86_64 with Python 3.12, CMake, Make, and a host C compiler installed:

```bash
python3 tools/bootstrap.py --local-toolchain --local-simulator-libs --device-compatibility-sdk
PLAYDATE_SDK_PATH="$PWD/.tools/PlaydateSDK-2.2.0" bash tools/build-game.sh
bash tools/test.sh
bash tools/run-simulator.sh build/game-2.2.0/simulator/sopwith.pdx
```

The bootstrap creates `.tools/` and downloads the SDKs and selected dependencies there; build and test commands create `build/`. These directories are generated locally and are absent from a fresh checkout. See [Fresh Linux setup](#fresh-linux-setup) for system packages and Simulator runtime requirements.

The game builder defaults to SDK 2.2.0 **only when `PLAYDATE_SDK_PATH` is unset**. Explicitly select it if your shell previously sourced `tools/env.sh`, which defaults to 3.1.2. SDK 3.1.2's Linux Simulator can load the native library built against 2.2.0. Use SDK 2.2.0 for bundles targeting Playdate OS 2.2.0.

Outputs:

- `build/game-2.2.0/simulator/sopwith.pdx`: Linux Simulator package (`pdex.so`).
- `build/game-2.2.0/device/sopwith.pdx`: console package (`pdex.bin`, `pdxversion=20100`).
- `build/tests/port_tests`: engine/runtime sanitizer tests, also used by `port_stress`.
- `build/tests/profile_tests`: portable persistence, tone, and marker tests.
- `build/tests/app_tests`: actual app and SDK file-adapter tests with a fake SDK service table; requires installed SDK headers. Without them, CMake explicitly skips this suite. See [TESTING.md](TESTING.md) for CI and limitations.

The root CMake build uses the external SDK's `playdate.cmake` compiler/linker settings and stages all generated assets in the build directory. No native binary is written into tracked `Source/`. Launcher PNGs under `Source/launcher/` are copied into both staging directories and compiled by `pdc`; see [launcher artwork](LAUNCHER_ART.md) for regeneration. Linux/ARM are verified; native macOS/Windows remain untested.

For hardware installation from WSL, see [USB installation from WSL](#usb-installation-from-wsl). The separate SDK probe remains available for diagnostics.

## Package a review ZIP

After building the candidate and updating [`REVIEWER_INSTRUCTIONS.txt`](REVIEWER_INSTRUCTIONS.txt) for its version:

```bash
python3 tools/package-review.py
```

This prepares the ZIP with the public repository URL. It works offline and states that matching source publication is pending; it does not publish, commit or verify remote source. Once the matching source is published to the [source repository](https://github.com/Suvitruf/sopwith-playdate), use its full commit SHA to verify and link that exact revision:

```bash
python3 tools/package-review.py --source-commit "$(git rev-parse HEAD)"
```

This packages the existing device bundle without rebuilding or changing it. The output is `build/releases/sopwith-playdate-<version>-build<number>.zip`, with a companion `.zip.sha256` file. The archive contains `sopwith.pdx/` at its root, `README_REVIEW.txt`, author/license notices, `SOURCE_CODE.txt` with the repository URL and optional links to the selected commit and source ZIP, and checksums for the packaged files. Source files are not bundled in this ZIP. With `--source-commit`, the packager downloads the source anonymously and compares code, build inputs, tests and license files with this checkout; unpublished or mismatched revisions fail. Repository documentation may differ. That verification requires network access. Keep the matching source publicly available while distributing this build, and retain the source link alongside the game.

Build the game again before packaging if game code has changed since compilation. The source check does not prove an existing binary was compiled from this checkout. SDK installations, Git metadata, game saves and development logs are excluded from the source publication and review ZIP.

The packager checks matching bundle/source version metadata, reviewer-note version, archive integrity and file contents. It does not submit the ZIP or establish Catalog approval or completion of the hardware acceptance matrix.

## Shell environment

After running the bootstrap, run this in each Bash session to select the installed tools and build the SDK probe:

```bash
source tools/env.sh
pdc --version
arm-none-eabi-gcc --version
bash tools/check-sdk.sh
```

The environment script selects the local SDK unless `PLAYDATE_SDK_PATH` is already set. It adds SDK and local compiler binaries to this shell's `PATH`; it does not edit shell startup files. The Simulator wrapper applies its supplemental library path only to the Simulator process.

## Fresh Linux setup

The automated download script supports **Linux x86_64**. Its optional local compiler/GUI packages are pinned specifically for **Ubuntu 24.04**. It requires Python 3.12 or equivalent backported tar extraction filters, HTTPS access, and `dpkg-deb` for local Debian packages. Reserve about **1.5 GB** for the currently selected tools and archives, plus build space.

On an ordinary Ubuntu machine, install prerequisites and runtime dependencies using its package manager:

```bash
sudo apt-get update
sudo apt-get install --no-install-recommends \
  build-essential cmake python3 ca-certificates \
  gcc-arm-none-eabi libnewlib-arm-none-eabi \
  libgtk-3-0t64 libasound2t64 libwebkit2gtk-4.1-0
python3 tools/bootstrap.py --device-compatibility-sdk
source tools/env.sh
bash tools/check-sdk.sh
```

If system installation is unavailable and the host C tools are already present, download the local toolchain and supplemental Simulator libraries:

```bash
python3 tools/bootstrap.py --local-toolchain --local-simulator-libs --device-compatibility-sdk
source tools/env.sh
bash tools/check-sdk.sh
```

The local GUI package set supplements an existing Ubuntu desktop installation; it is **not** a complete Linux root filesystem. On a minimal fresh image, install normal GUI runtime dependencies with the package manager. `ldd` reports any remaining missing shared libraries. Package names differ outside Ubuntu; use the SDK's platform instructions.

`tools/dependencies.json` pins URLs, versions, and SHA-256 hashes. The SDK and upstream hashes were calculated from the official downloads received during setup; they are reproducibility pins, not publisher signatures. Debian package hashes were checked against local APT repository metadata. The bootstrap downloads to a temporary file, verifies its hash, and extracts under ignored `.tools/`. Existing cached downloads are also verified.

The SDK download is subject to the [Playdate SDK license](https://play.date/dev/sdk-license/). Its Linux `setup.sh` installs system desktop/MIME integration and USB udev rules using root privileges. The bootstrap does not run it. Direct compilation and executable launching use the local SDK without that integration. To add system integration on your own machine:

```bash
source tools/env.sh
sudo bash "$PLAYDATE_SDK_PATH/setup.sh"
```

That optional command changes system configuration; it is unnecessary for the compiler checks.

## Build products and Simulator

```bash
bash tools/check-sdk.sh
bash tools/run-simulator.sh
# Or open another bundle:
bash tools/run-simulator.sh /absolute/path/to/game.pdx
```

The build script copies original probe sources into `build/sdk-smoke/project/`, configures host and ARM builds separately, and packages:

- `build/sdk-smoke/output/simulator/sdk_check.pdx` — contains Linux `pdex.so`.
- `build/sdk-smoke/output/device/sdk_check_device.pdx` — contains device `pdex.bin` and may also include the staged host library.

Expected probe behavior: a text screen identifies the SDK check and states the game is not implemented; holding A inverts the screen. The script verifies compilation and bundle files, not interactive behavior. The probe's bundle ID is for development only.

Panic's helper writes native staging files into `Source/`; the script therefore builds the two targets sequentially. If switching SDK versions or compilers, use fresh build directories rather than reusing an incompatible CMake cache. Never commit `.tools`, `.cache`, `build`, or SDK binaries.

## WSL and other hosts

WSLg or another working Linux GUI session is needed for the Linux Simulator. A running process with no output does not prove the game opened. Audio/display sockets can be blocked inside an automation sandbox; run the wrapper in a regular WSL terminal to diagnose GUI behavior. Do not disable WebKit security features as a routine workaround.

For display troubleshooting, `SDL_AUDIODRIVER=dummy GDK_BACKEND=x11 bash tools/run-simulator.sh` selects X11 and disables audio. This diagnostic mode cannot verify sound. See [verification status](TESTING.md#current-verification-status) for completed Simulator and hardware checks.

Windows-native Simulator requires a Windows build (`pdex.dll`), not WSL's Linux `.so`. For native Windows, install the Windows SDK, Visual Studio C tools, CMake, and GNU Arm toolchain, and set `PLAYDATE_SDK_PATH`. On macOS, install its SDK package and Xcode command-line tools; follow the SDK's ARM toolchain setup. The project's Linux bootstrap/check wrappers have not been validated on those platforms.

The probe CMake project supports the SDK's native platform branches. Copy `tools/sdk-smoke` into a disposable build workspace, then configure with `cmake -S <probe-copy> -B <host-build>`; configure a separate device directory using `-DCMAKE_TOOLCHAIN_FILE=<SDK>/C_API/buildsupport/arm.cmake`. See the official [C build documentation](https://sdk.play.date/3.1.2/Inside%20Playdate%20with%20C.html) for each host's compiler environment.

Hardware sideloading requires a physical console. The SDK documents uploading from the Simulator's Device menu. From WSL, the Windows SDK's `pdutil.exe` can also reach a Windows-connected console without WSL USB passthrough; see [USB installation from WSL](#usb-installation-from-wsl).

## SDK probe for Playdate OS 2.2.0

The SDK probe defaults to SDK 3.1.2. To build it for Playdate OS **2.2.0**, download and select the compatibility SDK. Use the matching SDK instead of editing the generated bundle's version fields:

```bash
python3 tools/bootstrap.py --device-compatibility-sdk
PLAYDATE_SDK_PATH="$PWD/.tools/PlaydateSDK-2.2.0" bash tools/check-sdk.sh
```

This creates `build/sdk-smoke-2.2.0/output/device/sdk_check_device.pdx`. The script also adapts the older SDK's package output layout. SDK 3.1.2 build products remain in `build/sdk-smoke/`. Compatibility builds have their own CMake caches.

## USB installation from WSL

Install the Windows Playdate SDK separately; the Linux bootstrap does not install `pdutil.exe`. With Windows interoperability enabled in WSL, set the utility path to your Windows SDK installation. Replace the example path below with its actual WSL path:

```bash
sopwith_pdutil="/mnt/c/path/to/PlaydateSDK/bin/pdutil.exe"
"$sopwith_pdutil" install "$(wslpath -w "$PWD/build/game-2.2.0/device/sopwith.pdx")"
"$sopwith_pdutil" run /Games/sopwith.pdx
```

`install` temporarily switches the console to Data Disk mode, replaces the matching bundle, and ejects the volume. Close other applications that hold its serial port first. Wait for USB re-enumeration before running the second command.

To install the compatible SDK probe instead, build it as described above and use the same utility:

```bash
"$sopwith_pdutil" install \
  "$(wslpath -w "$PWD/build/sdk-smoke-2.2.0/output/device/sdk_check_device.pdx")"
"$sopwith_pdutil" run /Games/sdk_check_device.pdx
```

Wait for the console to reappear between these commands as well. This installs the SDK check, not a Sopwith game build.

Read live USB logs using the included helper. Replace `COM3` below with the Playdate port shown in Windows Device Manager under **Ports (COM & LPT)**:

```bash
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass \
  -File "$(wslpath -w "$PWD/tools/device-console.ps1")" \
  -PortName COM3 -Seconds 45
```

Add `-Query version` to query firmware details. The helper validates the selected port's Playdate USB vendor/product ID and closes it after the requested interval. The PowerShell policy option applies only to that process. Logs from `version` can contain the device serial number; keep raw logs in ignored `build/logs/`, and omit that identifier from shared reports.

Build 3 logs frame counts every five seconds, button transitions, crank sample counts, docking state, and pause/resume events. Holding A inverts the display. This confirms individual SDK services; it is not a game performance benchmark. See [verification status](TESTING.md#current-verification-status) for completed observations and remaining checks.

## Upstream reference build

The optional desktop reference compares the port with the unmodified upstream engine. `--fetch-upstream` downloads the pinned source into `.cache/research/`, creating that directory locally; it is not included in the checkout or required to build the Playdate game. On Ubuntu 24.04, build the reference directly with GCC and SDL2:

```bash
python3 tools/bootstrap.py --fetch-upstream --local-desktop-libs
bash tools/build-reference.sh
bash tools/test.sh
python3 tools/check-reference.py
```

The local SDL2 development package is checksum-pinned; an installed SDL2 runtime is also required. Alternatively use system `libsdl2-dev` with `sdl2-config`. The script builds `build/reference/sopwith` and a 300-tick trace driver, with isolated preferences under `build/reference/`. `tools/check-reference.py` also requires `bash tools/test.sh` to have built the port harness. Run it in a normal terminal if sandbox tracing prevents LeakSanitizer from running.

The reference build uses every original top-level C source and the original SDL backend at the pinned revision. Only its build configuration and separate trace-driving executable are supplied by this project. For interactive desktop play, run `build/reference/sopwith -c`; no original source edits are needed.

## Troubleshooting

| Symptom | Action |
| --- | --- |
| `pdc` not found / missing `pd_api.h` | Source `tools/env.sh`; verify `PLAYDATE_SDK_PATH`. |
| Missing `arm-none-eabi-gcc` | Install ARM GCC plus Newlib, or use `--local-toolchain`. |
| `stdio.h` / `libc.a` missing with local ARM GCC | Rerun bootstrap; it creates local Newlib include/library links normally supplied by Debian alternatives. |
| Missing `libwebkit2gtk-4.1.so.0` | Install WebKitGTK 4.1 or use the tested local library supplement and launch through the wrapper. |
| Cannot open display / audio socket denied | Check the regular desktop/WSLg session and sandbox access. Compilation can still work. |
| Checksum mismatch | Keep the pin; remove the damaged archive and retry. Review actual upstream changes before updating the lock file. |
| Download URL unavailable | Use the recorded version from an official archive; verify hashes. Do not silently download `latest`. |
