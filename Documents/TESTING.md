# Verification strategy

The prototype has host sanitizer tests, an unmodified upstream reference comparison, Simulator/ARM builds, and an initial physical-device playtest. Broader gameplay acceptance remains open; the summary below distinguishes completed checks from remaining work in the [roadmap](ROADMAP.md).

## Current verification status

As of version **0.4.0 build 9**:

- Linux Simulator and ARM bundles compile with SDK 2.2.0. Native Windows and macOS Simulator builds have not been verified.
- Linux Simulator checks covered title, flight, menus and settings reload with dummy audio; they do not verify audible sound.
- The 0.4 score/save changes passed all four host sanitizer entries, including a 12,000-tick stress fixture and repeated-save fault injection. The independent comparison matched 300 seeded ticks against the unmodified desktop engine.
- Initial hardware playtests confirmed title, flight, sound and repeated flips in the earlier 0.2 prototype. These observations do not complete the current full mission, loss/retry, landing or control acceptance matrix.
- Build 8 with `com.apanasik.sopwith` passed a short device title-state startup check. Its separate data directory does not import earlier bundle IDs.
- Build 9's launcher animation was confirmed on a physical Playdate running OS 2.2.0. Both native packages contain the complete artwork and animation sequences; see [LAUNCHER_ART.md](LAUNCHER_ART.md).
- Repeated saves/relaunch on hardware, Daily/All-time screen acceptance, interrupted writes, sustained combat performance and total device memory remain open. A remote CI run has not yet been observed.

These observations are bounded checks. Record new results against the exact build and distinguish automated fixtures, Simulator checks and device tests.

## Checks available now

Run these commands from the repository root after [development setup](SETUP.md#build-the-game) and [upstream reference setup](SETUP.md#upstream-reference-build). The SDK and reference source are downloaded by those steps; they are not included in the checkout. `bash tools/test.sh` can run the three SDK-independent entries without either download.

```bash
bash tools/test.sh
bash tools/build-reference.sh
python3 tools/check-reference.py
PLAYDATE_SDK_PATH="$PWD/.tools/PlaydateSDK-2.2.0" bash tools/build-game.sh
bash -n tools/*.sh
python3 -m py_compile tools/bootstrap.py tools/check-reference.py
python3 tools/build-launcher.py --check
git diff --check
git diff --cached --check
```

The test wrapper runs three SDK-independent CTest entries (`port_tests`, `port_stress`, `profile_tests`) and, when SDK headers are installed, `app_tests`. Without headers, CMake explicitly reports that the application suite was skipped. This is not a four-suite pass. Set `PLAYDATE_SDK_PATH` or install the pinned compatibility SDK to include it.

`port_tests` checks short presses, held controls, B chords, pause clearing, opposing directions, crank thresholds/docking, clock wrap and catch-up bounds, framebuffer stride/margins/polarity, clipping and unchanged collision masks. Seeded engine runs at 17/33/100/250 ms callback intervals produce identical world state. Tests also cover repeated mission allocation stability, full shutdown cleanup, win/loss transitions and a novice takeoff/auto-return/refill with zero crashes.

`port_stress` executes 12,000 seeded ticks across repeated missions and difficulty levels, mixing throttle, guns, bombs, flips, and return-home. It checks object-list integrity, bounded tracked heap usage and zero engine allocations after shutdown. This represents 20 minutes of simulation time, accelerated on the host; it is not a 20-minute hardware endurance or performance test.

`profile_tests` exercises [save validation/recovery](STORAGE.md), future-version protection, partial/corrupted writes, failed verification/rename, generation wrap, v1 migration, daily/all-time sorting and GMT rollover, tone frequency/amplitude/fade bounds, and clipped faction markers. `app_tests` runs the actual app and storage adapter against fake SDK service tables from real SDK headers. It drives navigation, persisted preferences/scores across reinitialization, disabled-crank behavior, pause/lock/audio cleanup, save failure paths and file-handle cleanup. Its SDK fake rejects rename over existing files, and ten rotations plus unlink/rename failure checks cover the build 6 device regression. Injected results are fixtures, not full human mission victories. Real filesystem checks remain separate.

The novice refill fixture deliberately supplies a fuel/ammo deficit because novice mode has unlimited ammunition. The Dogfight trace independently observes bullets, bombs and crash/respawn behavior. Win/loss fixtures force the end condition; they do not prove a complete mission can be won by playing.

The reference comparison replays 300 ticks with seed 12345 through real unmodified desktop/SDL code and compares player fields plus a hash of explicit object fields and terrain at **every** tick. It excludes rendered pixels and audio: console help differs from keyboard help, and speaker emulation is intentionally preliminary. It is one regression fixture, not exhaustive equivalence.

AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer are enabled by the test wrapper. Some automation sandboxes trace processes, which makes LeakSanitizer fail before completion; rerun in a normal terminal/outside that tracing sandbox. Do not mistake that diagnostic for a passed leak check or silently disable sanitizers in reports.

The SDK check builds an original C program as a Linux shared library and as an ARM bundle. It checks that `pdex.so` and `pdex.bin` are nonempty. It does not execute device instructions. Launch the probe with `bash tools/run-simulator.sh`; verify its text screen and A-button inversion separately. See [SETUP.md](SETUP.md) for the SDK and host requirements.

For a Playdate running OS 2.2.0, use the [compatible SDK probe](SETUP.md#sdk-probe-for-playdate-os-220) and [USB installation instructions](SETUP.md#usb-installation-from-wsl). Probe build 3 emits `SDKCHECK` frame, input, crank, and lifecycle logs. Record hardware observations with the game version, platform and test conditions. The probe's frame rate does not establish game performance.

## Continuous integration

[`.github/workflows/build.yml`](../.github/workflows/build.yml) runs on pushes, pull requests and manual dispatch. Both jobs use Ubuntu 24.04 and a commit-pinned [official checkout action](https://github.com/actions/checkout).

- `host-tests`: script syntax, launcher-artwork consistency and three SDK-independent sanitizer entries; no SDK download.
- `sdk-build`: checksum-pinned SDK 2.2.0 plus toolchain/upstream acquisition, Simulator and ARM packaging, all four sanitizer entries, and the independent 300-tick desktop comparison. SDL2 development packages come from the Ubuntu runner's package manager.

SDK headers and binaries remain external and are not uploaded as CI artifacts. Headless CI builds the native Simulator library; it does not run interactive Simulator or physical-device tests. The workflow is implemented and its commands have been exercised locally; no GitHub workflow run has been observed yet.

## Broader regression coverage

| Area | Meaningful checks |
| --- | --- |
| Simulation timing | Equal world state for the same seeded command stream under different callback intervals; 100 ms steps; bounded long-stall behavior; timestamp wrap. |
| Input | A tap entirely between world ticks is consumed once; held inputs persist; chord release does not bomb; paused inputs are discarded; catch-up does not repeat one-shot commands. |
| Crank | Docking, negative rotation, dead zone, accumulated partial rotation, range clamps, and button precedence. |
| Pixel output | All index values, byte boundaries, first/last pixel, 52-byte stride, bit polarity, margins, padding, clipping, and inclusive dirty rows. |
| Collision | Same result for source sprite pairs before/after presentation changes; dither holes never alter occupancy. |
| Determinism | Fixed-seed flight/combat traces with explicit state fields; avoid hashing struct padding or pointer addresses. Compare against the pinned desktop build. |
| Storage | Missing, truncated, future-version, invalid-range, oversized, and failed-write cases; previous save survives a failed replacement. |
| Audio | Zero divisor/silence, valid frequency changes, pause/restart/mute, and bounded callback work. Compare sequencing separately from sample-generation fidelity. |
| Lifetime | Repeated title/mission/results/settings/restart paths do not leak objects, callbacks, or memory. |

Host-side adapter tests should run without Playdate hardware. Use sanitizers for host builds where supported, especially for clipping, parser input, allocation, and source-import changes. Add fixtures from actual observed behavior; avoid tests that merely duplicate the implementation's calculations.

## Manual gameplay acceptance matrix

Run against the desktop reference, then Simulator, then physical Playdate. Mark each platform's result separately.

| Scenario | Required observation |
| --- | --- |
| Takeoff | Throttle, pitch, speed, and runway behavior match the reference. |
| Maneuvering | Turn/flip behavior, stall, recovery, and direction remain consistent. |
| Dogfight | Computer opponents engage correctly; gun hits and damage agree with collision rules. |
| Bombing | Correct release, trajectory, target damage, terrain effects, and score. |
| Landing | Approach, touchdown, return-home, refill/repair behavior match the baseline. |
| Losing a life | Crash, respawn, counters, and eventual loss screen complete. |
| Winning | Original total remaining-life bonus is applied immediately; level progression and return paths stay responsive. No blocking score animation. |
| Menus | Every screen supports D-pad/A/B navigation without keyboard input. |
| Pause and lock | No simulation catch-up burst, stale chord, uncommanded bomb, or stuck tone. |
| Persistence | Settings and positive finished-mission scores survive exit/relaunch; corrupt data falls back to the other generation/defaults; newer saves are protected; failed saves can be retried. |
| Monochrome readability | Player, opponents, targets, bullets, runway, map markers, and HUD are distinguishable in motion. |
| Long session | Repeated missions/restarts and heavy effects stay within memory/time budgets. |

Five-second game logs now include `heap`, `peak`, and `allocs` for engine payloads in addition to callback/tick counts and maximum callback duration. Those omit SDK allocations, static buffers, stack, and tracking-header overhead; do not present them as total device RAM.

For device measurements, record console/OS, SDK/build revision, input preset, scene, session length, frame-time distribution and peaks, heap usage/growth, and any stack diagnostics available. Include a heavy-effects scene; a stationary title screen is insufficient.

## Release acceptance

Before marking an MVP complete, require a fresh hardware install, one full successful mission, one loss/restart cycle, docked and undocked control runs, save reload, pause/lock recovery, and performance evidence. Check the package metadata and corresponding source/build instructions against the exact candidate.

Do not claim full coverage if a console is unavailable. Record the missing device checks and leave their roadmap items open.
