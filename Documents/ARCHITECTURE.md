# Port architecture

The 0.4 prototype is implemented in native C11 using Playdate SDK 2.2.0 APIs. Its original engine baseline and compatibility edits are recorded in [the import ledger](../Sources/core/PORTING.md).

## Modules

| Path | Responsibility |
| --- | --- |
| `Sources/core/` | Flight, computer opponents, objects, collision, indexed drawing, built-in content and tone sequencer. Desktop-only source is retained but excluded from the build. |
| `Sources/port/engine.c` | `Engine_Init`, `Engine_Start`, `Engine_Step`, `Engine_Shutdown`; original command ring and indexed framebuffer. |
| `Sources/port/runtime.c` | SDK-independent clock, button/crank arbitration, monochrome conversion and bounded integer formatting. |
| `Sources/port/profile.c` | Versioned settings/scores, validation, recovery, and two-slot save transactions through an abstract I/O table. |
| `Sources/port/tone.c` | Bounded square-wave synthesis with volume ramping. |
| `Sources/port/markers.c` | Clipped player/faction marker overlay; never changes indexed sprites or collision data. |
| `Sources/port/hud.c` | Resource readouts in the side/bottom margins, using a copied resource summary and the upstream font; no engine mutations or allocations. |
| `Sources/port/pause_panel.c` | Read-only screen/resource snapshot and control hints for the native system-menu image. |
| `Sources/playdate/storage.c` | SDK file-service adapter with checked open/read/write/flush/close/rename operations. |
| `Sources/port/services.h` | Allocator, fatal error and monotonic time boundary. |
| `Sources/playdate/main.c` | SDK entry point, display callback, screens, system menu, audio source, lifecycle and service implementation. |
| `Source/` | Bundle metadata and launcher artwork. Binaries are staged under each separate build directory. |
| `tests/` | Host sanitizer tests and an independent driver for the unmodified desktop reference. |

The core does not include `pd_api.h`. Only the backend calls SDK services. The compiled source list is explicit in the root CMake file; no SDL, sockets, desktop filesystem, keyboard menu, text-entry loop, or custom mission parser is linked into the device build.

## Lifecycle and timing

`kEventInit` loads the profile, creates the engine and audio source, and registers a 30 Hz update callback. Screens are `TITLE`, `FLIGHT`, `RESULTS`, `OPTIONS`, `SETTINGS`, `CONTROLS`, `SCORES`, `CREDITS`, and `LEAVE_FLIGHT`. A starts Dogfight; B starts Practice. Results supports next mission/retry or return to title. Restart, Sound, and Options occupy exactly three system menu items. Options pauses flight and leads to the other screens. Leaving an unfinished mission through Options requires confirmation; the explicit Restart system action remains immediate.

The world advances at **10 Hz** using unsigned millisecond subtraction and a 100 ms accumulator. A callback may run at most three ticks; elapsed time beyond 300 ms is discarded. Pausing or changing screens clears timing debt, crank residue and input latches. Buttons held through the transition are blocked until released. Menu/lock events silence sound; resume/unlock reset both clocks.

Each tick produces one local command in the original ring, advances its timestamp, then executes **movement → drawing → collision → sound**. Between ticks, presentation uses the last completed indexed frame. Drawing still has engine sound side effects and is therefore not repeated at display cadence.

The bounded port `swrestart` awards the original total remaining-life bonus immediately, retains score on a win, and sets a flag for the app's results screen. It does not run the desktop blocking bonus animation or keyboard high-score entry. The original crash/respawn and auto-return logic remain in the core.

## Memory and services

Core checked allocation and free use `Port_Realloc`/`Port_Free`, backed by SDK `system->realloc` on both Simulator and hardware. SDK ARM startup glue remains external. The adapter avoids duplicate global allocator overrides. The backend stores an aligned size header for each engine allocation and reports live bytes, lifetime peak bytes, and allocation count in five-second telemetry. These count requested engine payloads, excluding header overhead, SDK-owned memory, static buffers and stack. Host tests independently track outstanding allocations.

Generated symbols live for the application lifetime. The indexed framebuffer is a static 64,000-byte array. The original engine dynamically grows its object pool during gameplay; the pool is reused during a mission and all active/free/deleted lists are released on a new level. Shutdown frees objects, ground and generated symbols after audio detaches. There is not yet a hard object-pool allocation limit; heavy-scene heap/stack profiling is outstanding.

`Timer_GetMS` adapts SDK time for legacy display messages; scheduling uses `uint32_t` directly. Core assertions and allocation failures route to an SDK error instead of Newlib process exit. All numeric UI text uses bounded stack buffers; there is no per-frame formatting allocation. No dummy POSIX syscall layer is provided. Persistence uses SDK file services rather than Newlib stdio.

## Display

The core retains all four color indices, its 320-byte pitch, XOR behavior, coordinate system and untouched collision occupancy. The conversion places the 320×200 image at (40,20) in the 400×240 LCD, with a 52-byte output row stride, MSB-first bits, 0 black and 1 white. Margins and row padding are white, and the backend marks rows 0–239 updated.

After conversion and faction markers, the app passes current fuel, gun rounds, bombs, remaining aircraft and Practice's unlimited-weapons flag to `Port_DrawHUD`. It clears and draws only the side and bottom margins, preserving the indexed source, central image, top strip and row padding. It uses bounded integers and the existing 8×8 font, scaling weapon counts and the aircraft count to 16 pixels high. The fuel gauge has steady low/empty labels; no effect blinks. The original HUD remains visible. The app's existing top-strip mode/throttle drawing follows this overlay.

Index 0 is white. Nonzero pixels are black except checkerboard holes inside connected index-2 areas; thin edges and isolated pixels remain visible. Index-3 bullets remain solid. The top margin shows mode/throttle. `Vid_Update` snapshots up to 100 visible living aircraft, standing targets, and flying balloons before collision handling, matching the just-drawn frame. The final frame receives a player chevron, friendly open square, or enemy X, each 5×5 with a one-pixel white surround. Markers outside the viewport are omitted; surplus markers are dropped. Settings can disable faction markers while keeping the player indicator. The minimap retains its original monochrome conversion; it has no new semantic markers. Crowding and motion readability still need device playtesting. No display operation modifies collision data.

The native pause image is a separate 400×240 SDK bitmap allocated once at initialization. On `kEventPause`, the app copies the current screen and resource values into `PortPausePanel`, draws with the row stride returned by `getBitmapData`, and calls `setMenuImage` with offset zero. Content stays within the left 192 pixels beside the system menu. The bitmap remains alive through resume animations and is detached before being freed at termination. Allocation failure leaves the default system-menu image available. `Port_DrawPausePanel` uses the existing font and never steps the engine, redraws its indexed frame or alters the game framebuffer.

## Audio

The original tone priorities feed a quiet square-wave source at **1,193,280 / divisor Hz**. Zero divisors and frequencies at/above Nyquist are silent. The sequencer runs on a resettable rational 18.2 Hz clock, independently of world ticks, with bounded catch-up. The audio callback has no allocation, I/O, logging or engine-object access; it reads atomic phase and volume values and fills 44.1 kHz samples. Volume has five settings, 0–100% in 25% steps, independently of mute; default 50% matches the earlier amplitude. Gain ramps by eight units per sample, at most 400 samples (about 9 ms), to soften start/stop and volume changes.

Pause, lock, mute and restart request silence through that bounded fade. Termination removes it before engine cleanup. The first backend does not reproduce the desktop speaker filter, and its sound has only received an initial device listening check.

## Settings and scores

The portable profile owns sound/volume, pitch inversion, crank enable, faction markers, and five positive finished-mission scores per mode for both Daily and All-time. Daily buckets use the GMT day of the results transition; the v2 format preserves older v1 settings/all-time scores. It writes only when dirty at explicit settings/menu/results/lifecycle boundaries, never from the audio callback or on every frame. See [STORAGE.md](STORAGE.md) for the binary format, checked replacement, failure recovery, and forward-version protection. This is a profile, not a mid-flight game save.

## Deferred work

Custom missions, multiplayer, optional missiles/starbursts, wider rendering, and full desktop speaker-filter fidelity remain deferred. The current options, score, help and credits screens are implemented; broader physical-device acceptance is outstanding.

Performance targets remain 30 Hz display / 10 Hz physics, under 4 MiB game allocations, and headroom during heavy combat. The [verification summary](TESTING.md#current-verification-status) separates initial device observations from outstanding performance and memory acceptance.

Simulator and ARM builds use separate directories and external SDK compiler/startup/linker support. Linux `.so`, macOS `.dylib` and Windows `.dll` are host-specific; only Linux and ARM builds have been verified here.
