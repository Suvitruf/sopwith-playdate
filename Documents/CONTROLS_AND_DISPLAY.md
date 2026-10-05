# Controls and monochrome display

Status: **0.4 controls/settings implemented; earlier device flight, sound and repeated flip confirmed**. Practice HUD readability is confirmed, and the lives display uses a single large number. Broader combat, chord interaction, settings and crowded-scene readability checks remain pending. The original command semantics come from pinned upstream `video.c`, `video.h`, `sw.h`, and `swmove.c`; see the [upstream import ledger](../Sources/core/PORTING.md).

## Input goals

Pitch and firing must work together. Throttle, flip, bombing, and return-home must remain available with the crank docked. Read controls at every update, then retain short presses until the 10 Hz simulation consumes them. Never require players to time a tap to a simulation tick.

Current preset:

| Console input | Action | Engine command |
| --- | --- | --- |
| D-pad Up / Down | Pull up / push down relative to plane | `K_FLAPU` / `K_FLAPD` |
| D-pad Right / Left | Increase / decrease throttle while held | `K_ACCEL` / `K_DEACC` |
| Hold A | Fire gun at the original cadence | `K_SHOT` |
| Tap and release B | Drop one bomb | One latched `K_BOMB` |
| Hold B, press Left | Flip the plane once | One latched `K_FLIP` |
| Hold B, press Right | Engage return-home once | One latched `K_HOME` |
| Crank, when undocked | Optional increase/decrease throttle | Same throttle command path |
| Menu | Standard system pause/menu | Platform lifecycle; no fabricated game key |

Up/down here name the engine's relative pitch commands; they are not promises of screen-relative motion in every orientation. Settings → Pitch swaps those two commands when inverted. The original SDL controller's pitch direction differs from the default preset; device trials should check the preferred convention.

## Chord and event rules

The B modifier trades immediate bomb-on-press for a release action. This is an intentional input difference: upstream allows a held bomb command. It must be accepted in playtests before becoming the shipped default.

- On B press, begin a pending bomb gesture. Do not emit a bomb yet.
- While B is held, suppress horizontal throttle commands. The first new Left or Right press emits its chord action and marks the gesture consumed. **Release B before another chord.** Holding a chord must not repeat it; a new B+Left gesture flips back.
- B release emits one bomb only if no chord consumed the gesture. An ambiguous pre-held horizontal direction cancels the pending bomb and requires a fresh direction press for a chord; do not guess the player's intention.
- A remains independent: shooting while pitching or tapping B should work.
- Opposing horizontal/vertical inputs resolve to neutral. Define precedence for artificial simultaneous inputs in the Simulator.
- Latch bomb/flip/home until the next tick; consume each once even during a callback with multiple catch-up ticks. Preserve held A/pitch/throttle between ticks.
- Pause, lock, restart, and leaving a menu clear gestures and latches. Require a fresh B gesture after resume so releasing a previously held B cannot bomb.

If trials show frequent accidental actions or unacceptable bombing latency, revisit the preset before adding more actions. Alternative presets are a later decision; do not silently change the mapping in code.

## Crank behavior

Use relative rotation (`getCrankChange`), accumulate degrees, and translate threshold crossings into bounded throttle steps. The prototype uses **30 degrees per step**, ignores samples within ±0.1 degree, queues at most four steps, and consumes at most one step per world tick. Clamp to the engine's 0–4 throttle range, preserve residual movement, and cap per-tick changes. D-pad throttle takes precedence when used during the same sample.

Discard residual crank motion when docking/undocking or pausing, so opening the crank cannot unexpectedly accelerate. The crank does not steer the plane by default. Settings → Crank throttle disables it entirely. D-pad throttle remains available in either setting.

## Screens and settings

The title uses A for Dogfight and B for Practice. Results uses A for next mission/retry and B for title. Down opens Options from either screen. The system menu contains Restart, Sound and Options; Options also pauses flight.

Use Up/Down to choose an option, A to select, and B to return. Settings supports Left/Right or A to change sound, volume, pitch direction, crank throttle, and faction markers; B or Back saves changes. Controls has two pages selected with Left/Right. Mission scores uses Left/Right to switch Dogfight/Practice and Up/Down to switch local Daily/All-time; Controls, Scores, and Credits accept A or B to return. The local pilot label is `YOU`, avoiding keyboard name entry. [STORAGE.md](STORAGE.md) defines the score/save policy and recovery notices.

Return to title asks for confirmation when leaving a running mission. An unfinished mission is not saved or resumable. Restart in the system menu immediately starts a new campaign in the selected mode.

## Current display implementation

The full 320×200 frame remains at (40,20) on the 400×240 display, including its original HUD and minimap. Build 10 adds duplicate resource readouts in the 40-pixel side margins and 20-pixel bottom margin. Mode and throttle remain in the top strip. No camera, sprite, collision or world-coordinate changes are involved.

The left margin shows fuel as a percentage and a larger vertical gauge. Positive fuel rounds up to the next percentage point, so it never reads 0% while fuel remains. A steady `LOW` label appears at 20% or less; `OUT` appears at zero. Fuel is not a time estimate. The right margin shows `LIVES` and one large count, matching the original remaining-aircraft counter including the current aircraft. Build 11 removes the duplicate filled/empty indicators after device feedback. The counter changes when the engine accounts for a crash, preserving the original timing.

The bottom strip shows `AMMO` and `BOMBS` with numbers twice the original font size. Practice shows the original font's infinity symbol for both unlimited weapons; fuel and aircraft remain finite. The margin HUD is always shown during flight, uses the existing upstream font, and adds no setting or save-format change. Rendering reads the current resource values without advancing the engine or changing them. Automated checks cover clipping, bounds and read-only behavior. Build 10 hardware captures confirm both weapon-display modes, decreasing resources and updated aircraft counts after crashes. Selected device flight intervals measure approximately 30 callbacks/s and 10 ticks/s with a maximum callback of 15 ms; device feedback subsequently confirmed that fuel, weapons and lives are readable in Practice. Crowded-scene readability and worst-case performance remain pending. See [verification status](TESTING.md#current-verification-status).

The prototype uses white sky, black silhouettes and interior index-2 dithering. Thin edges and isolated pixels stay solid. Visible living aircraft, standing targets and flying balloons receive presentation-only 5×5 markers with a white surround: **V = player, open square = friendly, X = enemy**. The frame snapshot occurs before collisions, avoiding stale object pointers or markers drifting from the completed frame. Faction markers can be disabled; the player marker remains. Markers are clipped to the viewport and limited to 100 per frame. They do not alter the minimap.

The following acceptance checks remain open:

| Content | Current treatment | Acceptance requirement |
| --- | --- | --- |
| Player | Solid outline and stable player marker | Find the player quickly among other planes. |
| Opponents/factions | Interior dithering and explicit faction markers | Distinguishable in flight; evaluate the unchanged minimap separately. |
| Terrain/buildings | Dark outlines, selective static fill | Runway and target shapes remain legible. |
| Bullets/bombs/debris | Guaranteed visible silhouette | One-pixel objects never disappear solely because of a dither phase. |
| HUD | High-contrast symbols and text | Fuel, ammunition, score, and damage readable at native size. |

Faction cues use object metadata in a presentation-only overlay because palette indices alone do not reliably identify allegiance. Do not change source sprite occupancy or erase visual data in the collision assets.

Use static spatial patterns; avoid temporal dithering or deliberate flicker as the default. Determine pattern anchoring during motion tests: screen-anchored patterns are simple but may shimmer on moving sprites. Object-anchored treatments need a sprite-aware presentation path. Record the chosen tradeoff.

## Pixel correctness

For direct output, address `frame[y * LCD_ROWSIZE + x / 8]` with mask `0x80 >> (x & 7)`. The row stride is **52**, not `400 / 8`. Set a bit for white and clear it for black. Mark modified rows with inclusive endpoints.

Test all four indices, overlapping XOR sprites, terrain edges, screen corners, clipping, the HUD, one-pixel projectiles, padding, and margins. Compare collision results before and after any visual change. Do not expand world constants to 400×240 simply because the display is that size.
