# Contributing

This project implements a native C port of SDL Sopwith to Playdate. Start with the [setup guide](SETUP.md), [roadmap](ROADMAP.md), and [architecture](ARCHITECTURE.md). The 0.4 implementation is covered by local regression tests; hardware gameplay, save/reload, readability, and performance acceptance are the next focus.

## Scope of a change

Keep one reviewable purpose per change: lifecycle extraction, video conversion, a specific input behavior, audio adaptation, or a documented bug fix. Preserve the original gameplay unless an intentional deviation is described and evaluated. Avoid broad reformatting of imported files; it hides the differences needed for upstream comparisons.

Contributor guides, verification summaries and the roadmap go in `Documents/`. Game code belongs in `Sources/`; tooling belongs in `tools/`. These directory names are case-sensitive contracts. Update the nearest document when a command, input behavior, architecture boundary, or milestone status changes.

## Working with upstream

Use the full revision in `tools/dependencies.json`, not the moving branch name. Keep the original `AUTHORS`, license, and copyright notices when importing. Record the origin and scope of copied files and preserve a way to compare modifications with upstream. Do not place SDK files in the engine import.

Portable engine fixes should be separable from Playdate-specific code. Keep a modification ledger when importing; the current ledger is `Sources/core/PORTING.md`. Upstream contributions or maintainer contact are separate actions, not a prerequisite for local work.

## Code and review expectations

- Use C11 for new port code; follow the surrounding upstream style in adapted files.
- Keep SDK calls behind documented platform boundaries. Make sizes, bounds, coordinate transforms, and ownership explicit.
- Preserve 10 Hz simulation and the original collision data. Explain changes to command ordering, random seeds, or arithmetic.
- Return promptly from update/audio callbacks; use state instead of sleeps or busy waits.
- Check allocation, file, and SDK errors; define a usable fallback for optional settings or sound.
- Keep binary outputs and local tools ignored. Never include the SDK in a source or release archive.

For a proposed change, provide a short description of the player-visible or developer-visible problem, what now happens, relevant validation, and any unverified device behavior. For graphics changes, include comparative captures where possible. For physics/input changes, include the relevant trace or scenario results.

Do not mark a roadmap item complete merely because it compiles. Use [TESTING.md](TESTING.md) for appropriate evidence. Note the SDK/compiler versions and host/device used for validation. Simulator performance does not establish a hardware performance result.

## Bug reports and useful first tasks

A useful bug report includes revision, SDK version, host or device OS, reproducible steps, expected/actual behavior, input preset, and logs/captures if relevant. Report whether the crank was docked and whether pause/restart occurred before the problem.

Useful next contributions include hardware chord/combat/landing playtests, faction readability, device save/reload, and longer audio/performance checks. Check the roadmap first so work supports the next milestone.

Before distributing an engine derivative, follow the [licensing and provenance notes](LICENSING.md). Follow per-file notices and the source import ledger; do not infer SDK redistribution permission from the engine license.
