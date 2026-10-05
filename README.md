![Sopwith launcher animation](https://projects.cdn.aapanasik.com/sopwith/sopwith-launcher-350x155.gif)

# Sopwith for Playdate

A native C port of [SDL Sopwith](https://github.com/fragglet/sdl-sopwith): take off, dogfight, bomb targets, and land on Playdate's monochrome screen.

**Status:** prototype **0.4.0 (build 9)** adds an animated Home screen card and list icon. It includes local **Daily** and **All-time** mission scores and a fix for repeated saves on older device firmware. The bundle ID remains `com.apanasik.sopwith`, as in build 8. Settings and scores from builds before 8 are not automatically imported. Dogfight and Practice stay separate. Online Catalog scoreboards require registered boards and are not connected yet.

Build 9's launcher animation has been confirmed on a physical Playdate. Build 8 passed a brief title-state console check; full mission, storage-fix and performance acceptance on hardware remain open. See [launcher artwork](Documents/LAUNCHER_ART.md), [verification status](Documents/TESTING.md#current-verification-status), and [scoreboard registration](Documents/SCOREBOARDS.md).

## Start here

- [Development setup](Documents/SETUP.md) — SDK installation and verified build commands.
- [Porting roadmap](Documents/ROADMAP.md) — implemented features, remaining milestones and release criteria.
- [Contributor guide](Documents/CONTRIBUTING.md) — workflow and review expectations.
- [Architecture](Documents/ARCHITECTURE.md) and [controls and display](Documents/CONTROLS_AND_DISPLAY.md) — implemented interfaces and remaining design work.

From a fresh checkout on Ubuntu 24.04 x86_64 with Python 3.12, CMake, Make, and a host C compiler:

```bash
python3 tools/bootstrap.py --local-toolchain --local-simulator-libs --device-compatibility-sdk
PLAYDATE_SDK_PATH="$PWD/.tools/PlaydateSDK-2.2.0" bash tools/build-game.sh
bash tools/test.sh
bash tools/run-simulator.sh build/game-2.2.0/simulator/sopwith.pdx
```

The device bundle is `build/game-2.2.0/device/sopwith.pdx`. On the title screen, A starts Dogfight and B starts Practice. Right/Left changes throttle; Up/Down pitches; A fires; releasing B bombs. Hold B then tap Left to flip, or Right to fly home. **Release B before the next chord.** Menu → Options provides Settings, Mission scores, Controls, and Credits; Down opens Options from the title/results screen. In Mission scores, Up/Down switches Daily/All-time and Left/Right switches Dogfight/Practice. Settings and the five best positive completed-mission scores per mode and period are saved locally; unfinished missions cannot be resumed.

The bootstrap downloads the [Playdate SDK under Panic's license](https://play.date/dev/sdk-license/) and creates `.tools/` for local dependencies. Build commands create `build/` for generated output. Neither directory is included in the checkout. The bootstrap does not change system packages or shell startup files. See the [setup guide](Documents/SETUP.md) for prerequisite packages, other operating systems, and the optional upstream reference build.

## Repository layout

| Path | Purpose |
| --- | --- |
| `Sources/` | Imported engine, portable adapters, and Playdate application. |
| `Source/` | Bundle metadata and launcher artwork; generated packaging stays under `build/`. |
| `tests/` | Host regression tests and upstream trace driver. |
| `Documents/` | Contributor setup, architecture, controls, testing, roadmap and attribution. |
| `tools/` | Bootstrap, game/reference builds, tests, and SDK probe. |

The starting baseline is upstream commit `c4e034109e28a5bd1727645fe77b27430219e8b5`; game bundles use SDK **2.2.0** for compatibility with Playdate OS 2.2.0. The bootstrap also downloads SDK 3.1.2 for the Linux Simulator and SDK probe. Multiplayer is deferred. Attribution and distribution questions are tracked in [LICENSING.md](Documents/LICENSING.md).
