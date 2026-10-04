# Game source

- `core/`: imported SDL Sopwith engine and compatibility fixes; see [provenance and modification ledger](core/PORTING.md).
- `port/`: SDK-independent engine lifecycle, clock, input, conversion and service interfaces.
- `playdate/main.c`: native SDK entry point, screens, audio, system menu, allocation, and lifecycle.

Build with `bash tools/build-game.sh` from the repository root. The root `Source/` directory contains packaging metadata; native binaries are staged only under `build/`. Host regression tests live in `tests/`.
