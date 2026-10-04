#!/usr/bin/env python3
"""Compare the port against the pinned, unmodified desktop engine for 300 ticks."""
# SPDX-License-Identifier: GPL-2.0-or-later
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
env = os.environ.copy()
env.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", SDL_RENDER_DRIVER="software",
           XDG_DATA_HOME=str(ROOT / "build/reference/data"),
           XDG_CONFIG_HOME=str(ROOT / "build/reference/config"),
           UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")


def trace(command):
    output = subprocess.check_output(command, cwd=ROOT, env=env, text=True)
    rows = []
    for line in output.splitlines():
        # SDL's dummy renderer can print a diagnostic without a final newline.
        match = re.search(r"(\d+(?: -?\d+){8})$", line)
        if match:
            rows.append(tuple(map(int, match[1].split())))
    if len(rows) != 300 or [row[0] for row in rows] != list(range(1, 301)):
        raise RuntimeError(f"Incomplete trace from {command}: {len(rows)} rows")
    return rows


reference = trace([str(ROOT / "build/reference/trace")])
port = trace([str(ROOT / "build/tests/port_tests"), "--trace"])
for expected, actual in zip(reference, port):
    if expected != actual:
        raise SystemExit(f"Mismatch at tick {expected[0]}:\nreference={expected}\nport={actual}")
print("PASS: all 300 ticks match upstream object/terrain state; seed 12345 (rendering/audio excluded)")
