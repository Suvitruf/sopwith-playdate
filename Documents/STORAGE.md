# Settings and mission scores

Version 0.4 stores a small local profile using `Sources/port/profile.c` and `Sources/playdate/storage.c`. The portable module validates bytes and manages recovery; only the SDK adapter touches files. The format is independent of C structure layout, pointer width, and host endianness.

## Player-visible policy

Defaults: sound on, volume 50%, normal pitch, crank throttle enabled, faction markers enabled. Volume is independent of mute. Each mode, Dogfight and Practice, has separate **All-time** and **Daily** lists of five descending positive scores under the fixed local name `YOU`. These are local device records; no scores are sent to Panic. See [SCOREBOARDS.md](SCOREBOARDS.md) for Catalog registration and the online integration plan.

A score is recorded in both periods once when a mission reaches its results screen, on either a win or final loss. Zero/negative scores are omitted. Advancing after a win preserves the original cumulative campaign score, so later completed missions can add higher entries. Equal scores can occupy separate entries. Restarting or leaving an unfinished mission does not submit its score. There is no mid-flight save, resume, desktop-score import, or editable player name.

Daily scores are grouped by the UTC/GMT day on which the results screen is reached, using `getSecondsSinceEpoch() / 86400`. At midnight, yesterday's entries stop appearing immediately; the next positive finished score starts a fresh daily list for both modes. Merely viewing scores does not write files. All-time scores remain unchanged. A campaign can span midnight: its completed-mission cumulative score belongs to the finishing day. This is a daily scoreboard, not a seeded daily challenge. The local board trusts the device clock; adjusting the date changes which list is visible and a score on another date replaces the stored daily bucket. Previous days are not archived.

Changed profiles are saved on leaving Settings, changing the system Sound item, entering results, and relevant menu/pause/lock/termination boundaries. There is no per-frame save and no audio-thread I/O. A fresh launch with no changes does not write default files.

## Files and replacement

The SDK stores files relative to this game's data directory (`Data/com.apanasik.sopwith/` on device):

- `profile-a.dat` and `profile-b.dat`: alternating generations, one current and one previous.
- `profile.tmp`: temporary candidate, never treated as a committed save during loading.

Build 8 changes the bundle ID from `dev.sopwith.playdate` to `com.apanasik.sopwith`. This selects a new data directory; settings and scores from the earlier ID are not automatically imported. The earlier data remains in its original directory. Profile-format migration applies to records already inside the current game's data directory.

Load reads each slot into a bounded 105-byte buffer (one more than the valid record size). Validate each independently, then choose the newest valid generation using unsigned serial-number comparison, including wraparound. A corrupt slot falls back to the other valid slot; both missing/corrupt uses defaults. The UI reports recovered data. An I/O error or recognizable newer-version slot disables writes for that session, even when the other slot is readable, to avoid overwriting information this build cannot safely interpret. The UI then says changes stay in memory.

To save a dirty profile:

1. Encode generation + 1 into exactly 104 bytes (format v2).
2. Open/write `profile.tmp`, check the byte count, flush, and close. Always close an opened handle, including failed-write paths.
3. Reopen and compare the temporary file byte-for-byte.
4. If the **inactive** slot exists, open it explicitly from the data folder, close it, then remove it with checked nonrecursive `unlink`. Rename the verified temporary file into that now-empty slot; check the result. Never remove the active slot.
5. Reopen and verify that slot before changing the in-memory active generation and clearing dirty state.

The previous active slot is never modified by that transaction. Failure preserves dirty state and offers **Options → Retry save**. The SDK documents replacement by `rename`, but older device firmware rejects an existing destination. The explicit inactive-slot removal avoids that [confirmed API discrepancy](https://devforum.play.date/t/file-rename-errors-if-target-file-exists/22840). A failed removal or rename aborts the transaction. No claim is made that rename/flush has power-loss atomicity or storage-device durability guarantees. The retained generation and CRC provide recovery from incomplete/corrupt records, not a filesystem guarantee. Actual power-loss behavior remains untested. Do not delete or truncate the current slot to make a retry succeed.

The SDK has no structured file-not-found return for `open`. The adapter first lists the game data root to distinguish a genuinely absent file from failed access; listing/open/read/close errors are treated as I/O failures. No desktop filesystem fallback is used on device.

## Version 2 record

All multi-byte integers are little-endian. Exact length is 104 bytes. Version 1 records (60 bytes, used by 0.3) are still accepted and migrated in memory, preserving settings and all-time scores and starting with empty daily scores. Their first subsequent save uses v2 in the inactive slot, preserving the prior active v1 record until a later successful rotation.

| Offset | Size | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII `SWPD` magic |
| 4 | 2 | Format version, `2` |
| 6 | 2 | Record length, `104` |
| 8 | 4 | Unsigned generation counter |
| 12 | 1 | Flags: bit 0 sound, bit 1 crank, bit 2 inverted pitch, bit 3 faction markers |
| 13 | 1 | Volume step, 0–4 |
| 14 | 2 | Reserved, must be zero |
| 16 | 20 | Five Dogfight scores, unsigned 32-bit values constrained to 0…INT32_MAX |
| 36 | 20 | Five Practice scores, same constraints |
| 56 | 4 | Daily bucket: UTC days since 2000-01-01, or `0xffffffff` when unset |
| 60 | 20 | Five daily Dogfight scores |
| 80 | 20 | Five daily Practice scores |
| 100 | 4 | CRC-32 over bytes 0–99; reflected polynomial `0xedb88320`, initial/final XOR `0xffffffff` |

Each score array must be descending (ties allowed); zero fills unused entries. Reject unknown flag bits, nonzero reserved bytes, invalid volume, score overflow/order, bad CRC, short/oversized records, and unsupported old versions. The daily bucket must be within the uint32 epoch range (0–49710 days); an unset bucket requires empty daily lists. Recognizable versions greater than 2 are protected even when this build cannot verify their record layout.

For v1, bytes 0–55 have the same meanings, with version 1 and length 60, and its CRC occupies bytes 56–59. No historical date can be inferred from v1 scores, so they never populate Daily during migration. Build 6 treats a v2 slot as a future format and disables writes, protecting the newer data if a user downgrades.

Future format changes must supply migration or retain read-only protection. Do not reinterpret versioned reserved bytes or silently change lengths. Keep corruption and interrupted-write fixtures when changing this contract.

## Verification

`profile_tests` exercises binary validation, version protection, score sorting, daily boundaries, a frozen v1 migration fixture, generation wrap, recovery and transaction failures with an in-memory file service. `app_tests` drives the actual application and SDK adapter against SDK-shaped fake services, including open/write/flush/close/rename/read/list/unlink failures, settings reload, daily/all-time score reload, and handle cleanup. Its fake SDK now refuses rename over an existing destination, matching the device, and ten successive save rotations pass. SDK headers are required for the latter suite.

A real Linux Simulator save/exit/relaunch restored changed settings in 0.3. The 0.3 device then exposed repeated-save failures after generation 2. Build 10 passed twelve consecutive saves and repeated relaunches on an OS 2.2.0 device using the identical executable under a separate test bundle ID with a fresh profile. The final two device records were valid 104-byte v2 files with correct CRCs and generations 11/12; a changed sound setting survived relaunch. This verifies ordinary repeated saves while leaving the existing player profile untouched. Positive-score retention, upgrade/migration and interrupted-power checks remain pending. See [TESTING.md](TESTING.md#current-verification-status) for the verification summary and remaining checks.
