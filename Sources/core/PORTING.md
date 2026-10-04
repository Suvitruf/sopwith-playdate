# Upstream import and modifications

Imported 2026-10-04 from [SDL Sopwith c4e034109e28a5bd1727645fe77b27430219e8b5](https://github.com/fragglet/sdl-sopwith/tree/c4e034109e28a5bd1727645fe77b27430219e8b5). Every top-level `src/*.c` and `src/*.h` is retained here, along with upstream `AUTHORS`. The source archive URL and SHA-256 are pinned in `tools/dependencies.json`. The original archive remains available through `tools/bootstrap.py --fetch-upstream` for comparisons. No SDL backend or SDK implementation is copied into the port.

`COPYING.md` at the repository root is upstream's GPL version 2 text. Most source headers permit GPL-2.0-or-later. `font.h` is LGPL-2.1-or-later (A. Schiffler and Simon Howard); its license is in `LICENSES/LGPL-2.1.txt`. `yocton.c` and `yocton.h` have Simon Howard's ISC notice, retained verbatim. Newly written port code, tests, CMake and game build/reference scripts use GPL-2.0-or-later as indicated by their SPDX headers. SDK distribution review remains a separate release gate.

## Compiled subset

The root CMake file lists the actual translation units. Desktop configuration, high scores, keyboard menus, title implementation, networking, touch controls, and custom-map parser are retained as reference but excluded from game compilation. `SOPWITH_PORT` also excludes the desktop main loop, CLI initialization, custom-map loader, blocking text input, and blocking restart/exit paths inside otherwise compiled files. No filesystem or networking stub pretends those features work.

## Modification ledger

| File | Change |
| --- | --- |
| `std.h` | Route core allocation/free through explicit port services. |
| `swmain.c` | Exclude blocking process loop; allocation failures use platform errors; duplicate strings via the checked allocator. |
| `swinit.c` | Release object lists before level reset; use built-in mission; exclude desktop setup; bounded win bonus and restart flag; reset campaign progress. |
| `swend.c` | Exclude desktop process exit; port rejects an unexpected exit path explicitly. |
| `swgames.c` | Compile only original built-in content for the port. |
| `swgrpha.c` | Practice help describes console controls, the Options menu, and releasing B between chords; desktop notifications/help formatting excluded. Keyboard-help overlay after three crashes is absent in Dogfight. |
| `swtext.c` | Exclude blocking text entry; keep original font rendering. |
| `swstbar.c` | Format integer score without Newlib's process/stdio dependencies. |
| `swobject.c` | Use unsigned shifts for fixed-point bit packing; preserve two's-complement results while removing undefined negative left shifts. |
| `swsound.c` | Reset sound object references on restart; resettable rational 18.2 Hz scheduler; bounded sound catch-up; fix out-of-bounds note lookup for `R` (rest); explicit callback prototype. |
| `video.c` | Exclude desktop key polling; clip pixels, sprites and boxes before pointer arithmetic; terminate solid-ground loop for nonpositive heights. Keep indexed XOR and original inclusive box height. |

All other imported source files are byte-for-byte unchanged. New interfaces, scheduler/input/conversion, and the platform application are in `Sources/port/` and `Sources/playdate/`.

## Intentional differences and verification

World ticks remain 10 Hz, in movement → drawing → collision → sound order. Bomb release and B chords replace keyboard controls. Rendering is monochrome with player/faction markers; source collision pixels are untouched. A completed mission receives the original total life bonus immediately before the results screen rather than blocking through its desktop animation. SDK-backed settings and per-mode mission scores replace the desktop configuration/high-score format; there is no custom map or multiplayer UI.

`tools/check-reference.py` compares 300 seeded ticks with the separately built **unmodified** desktop sources. Its hash covers explicit object fields and terrain, excluding padding, pointers, rendering and audio. This is a bounded regression fixture, not proof that every original game path is identical. The [verification summary and test matrix](../../Documents/TESTING.md) distinguish completed checks from outstanding hardware acceptance.
