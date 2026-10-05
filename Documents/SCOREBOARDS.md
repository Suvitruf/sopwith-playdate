# Local scores and Catalog scoreboards

Researched 2026-10-04 against Panic's [Scoreboard API](https://help.play.date/catalog-developer/scoreboard-api/). Version 0.4 implements local Daily and All-time lists. Online submission/fetch is **not implemented or enabled** and no boards have been registered for this project.

## Current game behavior

Each Dogfight/Practice mode has five positive finished-mission scores for All-time and for the current GMT day. Practice stays separate because its rules include unlimited supplies. Options → Mission scores opens All-time; Left/Right switches mode, Up/Down switches period, A/B returns. The Daily screen explicitly says local. Dates follow the console clock and rollover occurs at 00:00 GMT. [STORAGE.md](STORAGE.md) defines migration, boundaries, repeated-save recovery and score semantics.

This is a date-filtered high-score list, not a daily seeded challenge. Daily challenge rules would require a separate gameplay decision and would use distinct boards to avoid comparing incompatible runs.

## Registering online boards

Panic's online scoreboard service is available to Season/Catalog games. An SDK installation, Playdate account, or USB-sideloaded prototype alone does not provide that access.

1. [Submit the game to Catalog](https://help.play.date/developer/submit-to-catalog/) for review. After acceptance, complete the necessary developer onboarding/access steps. If an accepted game is unavailable in your account, Panic's [publishing guide](https://help.play.date/developer/publishing-in-catalog/) directs developers to `catalog-dev@play.date`.
2. Sign in to the Dev portal and open **Catalog Developer**. Create boards associated with the game's exact `bundleID` in `pdxinfo`: `com.apanasik.sopwith` from build 8 onward.
3. Supply each board's display name, ID (without special characters), regular/daily type, and descending score order. Suggested IDs below are proposals, not registered resources.
4. Panic's instructions say to contact them to enable the daily feature on a specific board. Daily service boards reset at midnight GMT.
5. Give the implementation the confirmed bundle ID and actual board IDs. A password, account token, or API secret is not needed in the repository.

| Mode | Regular board proposal | Daily board proposal |
| --- | --- | --- |
| Dogfight | `dogfightalltime` | `dogfightdaily` |
| Practice, if published online | `practicealltime` | `practicedaily` |

Catalog submission and board registration must be completed through Panic's developer portal before online integration can be tested.

## Integration plan once boards exist

The SDK 2.2.0 headers expose `PlaydateAPI.scoreboards`, so the C interface is available at the game's current SDK baseline. Server compatibility with OS 2.2.0 still needs verification on the registered game.

Keep the profile lists usable offline. Route positive finished scores to the confirmed mode/period boards, preserving the current separate-mode policy. Fetch leaderboards only from a responsive menu state, with loading/error/cached-result labels; network calls may take ten seconds or more. Do not pause the physics callback waiting for a response. Limit concurrent work and follow SDK ownership by freeing callback score/list data with the corresponding scoreboard functions.

The service documents an outgoing cache for offline scores and cached reads. Do not claim an offline daily score reached its intended day without testing submission around midnight and delayed delivery; the exposed API accepts a board ID/value, not the run's date. Resolve those semantics with Panic before release. Local save errors and online submission errors must remain distinct in the UI.

## Testing registered boards

Register the Simulator to the developer's Playdate account. Simulator and physical-device leaderboards are separate test environments. Use USB deployment when testing on hardware: Panic warns wireless sideload changes the bundle ID, breaking the association with registered boards. See [USB installation](SETUP.md#usb-installation-from-wsl) for the WSL procedure.

Test submission, retrieval, personal best, offline cache, retry, app exit during an outstanding callback, GMT rollover, mode separation, and positive-value limits on the actual registered game. Follow [TESTING.md](TESTING.md) for local regression checks. No successful online call or global rank is claimed until this is done.
