# Sopwith port roadmap

Current version: **0.4.0 build 9**, bundle ID `com.apanasik.sopwith`. The native game and animated launcher artwork are implemented. Launcher animation is confirmed on a physical Playdate; full gameplay, persistence and performance acceptance remain open. See [TESTING.md](TESTING.md) for current verification and the acceptance matrix.

## Target experience

Take off, steer and flip the plane, fight computer opponents, fire and bomb, return home to land and replenish supplies, then advance or restart a mission. Preserve the original simulation and collision behavior, with readable monochrome graphics and controls usable with the crank docked.

The first release focuses on built-in offline missions. Network multiplayer, custom maps, missiles/starbursts and wider rendering remain deferred.

## Implemented foundation

- [x] Pin and import the upstream engine with attribution and a [modification ledger](../Sources/core/PORTING.md).
- [x] Provide external SDK/toolchain setup, Linux Simulator and ARM builds, host sanitizer tests and an independent desktop reference comparison.
- [x] Replace the desktop loop with bounded Playdate callbacks, 10 Hz world steps, lifecycle handling and a 320×200 indexed framebuffer converted to monochrome.
- [x] Adapt flight, weapons, flips, return-home and optional crank throttle; retain the original collision data and built-in missions.
- [x] Implement title/results/options screens, controls, credits, volume and presentation settings, plus local Daily/All-time scores for both modes.
- [x] Implement versioned two-slot profiles and the repeated-save workaround for older firmware, with fault-injection tests.
- [x] Add animated launcher card/icon artwork and a loading image, with reproducible [artwork generation](LAUNCHER_ART.md).

These features are implemented; their remaining hardware checks are listed below. Build success alone does not complete gameplay acceptance.

## 1. Complete gameplay and controls acceptance

- [ ] Complete takeoff, dogfight, bombing, crash/respawn, mission victory, advancing to another mission, final loss and restart on hardware.
- [ ] Verify approach, landing, return-home, refuelling and rearming.
- [ ] Exercise simultaneous pitch/fire, repeated B chords and bomb releases with the crank docked and undocked; check for unintended throttle or bombs.
- [ ] Verify Menu, lock/unlock, pause/resume and restart without input replay, simulation catch-up or stuck sound.
- [ ] Check player/faction markers, terrain, targets, bullets and HUD in motion.

Use the [controls contract](CONTROLS_AND_DISPLAY.md) and [manual test matrix](TESTING.md#manual-gameplay-acceptance-matrix).

## 2. Verify persistence, menus and sound

- [ ] Exercise at least ten successive saves and a relaunch on hardware to verify the repeated-save fix retained from build 7.
- [ ] Confirm settings and positive completed-mission scores survive relaunch; check Daily/All-time navigation and GMT rollover.
- [ ] Check profile migration, corruption recovery and interrupted writes on actual storage. Build 8 introduced a separate data directory; earlier bundle IDs are not imported automatically.
- [ ] Verify every menu and setting, including inverted pitch, crank disable, faction markers, mute and volume.
- [ ] Listen to engine, weapon, explosion and music cues during play and lifecycle transitions.

Format and recovery rules are in [STORAGE.md](STORAGE.md).

## 3. Measure performance and complete regression checks

- [ ] Profile CPU, heap, stack and frame times during heavy combat on hardware.
- [ ] Validate the targets of 30 display updates per second, 10 simulation ticks per second and under 4 MiB game allocations with sufficient headroom.
- [ ] Run repeated missions and restarts for a sustained hardware session.
- [ ] Run the full regression suite and desktop comparison after gameplay or platform changes, and observe a successful remote CI run.
- [ ] Optimize measured bottlenecks without changing original game rules.

Host stress tests and title-screen telemetry do not establish worst-case hardware performance or total device memory use.

## 4. Prepare the release

- [ ] Finalize supported OS, release version and increasing build number; retain the chosen bundle ID for save compatibility.
- [ ] Complete [licensing and provenance review](LICENSING.md) for the exact linked components and preserve author/component notices.
- [ ] Make the corresponding source and build instructions available and verify the review ZIP against the exact published revision.
- [ ] Test fresh installation, updating, data retention and sideloading.
- [ ] Write release notes describing intentional gameplay differences and known limitations.

The release requires a full successful mission, a loss/restart cycle, docked and undocked controls, save reload, pause/lock recovery and performance measurements. Publishing remains a separate action.

## Optional online scoreboards

Local Daily and All-time lists are implemented. Online integration needs Catalog access, boards registered for `com.apanasik.sopwith`, and daily-board enablement. See [SCOREBOARDS.md](SCOREBOARDS.md). Registration and online device verification remain separate from the offline release work.
