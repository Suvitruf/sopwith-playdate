# Launcher artwork

Build 9 adds artwork for the Playdate Home screen. The card has the game name on the right and a looping plane above terrain and three buildings on the left. List view has a small animated plane icon. The launcher owns playback; the game does not run a background animation or load these assets in its update callback.

## Files and playback

`Source/pdxinfo` sets `imagePath=launcher`. CMake copies `Source/launcher/` into each build's staging directory before running `pdc`, which compiles the PNG files to `.pdi`. The checked-in source images are opaque 1-bit PNGs.

| Path under `Source/launcher/` | Size | Purpose |
| --- | --- | --- |
| `card.png` | 350 × 155 | Unselected card and fallback. |
| `card-highlighted/1.png` through `48.png` | 350 × 155 | Selected card animation. |
| `icon.png` | 32 × 32 | List icon and fallback. |
| `icon-highlighted/1.png` through `48.png` | 32 × 32 | Selected list icon animation. |
| `launchImage.png` | 400 × 240 | Loading image, with the card at (25, 43). |

Both animation directories contain `animation.txt`, listing frames 1–48 in order and holding each for two launcher animation frames. There is no `loopCount`, so playback repeats while selected. The launcher determines the actual timing. The plane follows a circle using the game's 16 heading poses; the title, buildings and terrain remain fixed. List view uses a smaller circle. Static images match the first frame to avoid a jump on selection.

The layout follows [Panic's game metadata documentation](https://sdk.play.date/3.1.2/Inside%20Playdate.html#pdxinfo) and the equivalent documentation bundled with SDK 2.2.0. No SDK sample art is included.

## Rebuild and verify

The renderer needs only Python's standard library:

```bash
python3 tools/build-launcher.py
python3 tools/build-launcher.py --check
PLAYDATE_SDK_PATH="$PWD/.tools/PlaydateSDK-2.2.0" bash tools/build-game.sh
```

Use `--output <directory>` for an alternate render destination. `--check` compares every expected PNG and animation sequence without writing files; CI runs it to detect stale generated artwork. Normal builds use the checked-in images and do not require an image library or asset-generation step. Change the renderer and regenerate when adjusting the layout or animation.

Verification for build 9: both SDK 2.2.0 Simulator and ARM bundles contain all 101 compiled artwork/sequence files. PNG dimensions, monochrome mode, contiguous frame numbering and the stationary title were checked; a pose sheet was visually inspected. The official USB utility installed the build, and the user confirmed "Yes, the animation works" when asked to select Sopwith on Home. Separate card/list-view timing and long-duration checks were not recorded.

## Source and attribution

- Plane poses and buildings come from `Sources/core/swsymbol.c`; the renderer follows its quarter-turn transform. The buildings are two hangars and a tent.
- Terrain samples come from `original_ground` in `Sources/core/swgames.c`.
- Letterforms come from `Sources/core/font.h`, scaled with integer pixels.
- Composition, motion path and the renderer are defined in `tools/build-launcher.py`.

The imported sprites and terrain retain the upstream GPL-2.0-or-later notices; the font retains the A. Schiffler/Simon Howard LGPL-2.1-or-later notices. See `COPYING.md`, `LICENSES/LGPL-2.1.txt`, and the original source headers. Keep these inputs and the generator available with the source distribution.
