#!/usr/bin/env python3
"""Install pinned development tools locally; never invokes sudo or changes dotfiles."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
LOCK = json.loads((ROOT / "tools/dependencies.json").read_text())


def download(item):
    cache = ROOT / ".tools/downloads"
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / item["filename"]

    def digest(path):
        with path.open("rb") as stream:
            return hashlib.file_digest(stream, "sha256").hexdigest()

    if not target.exists():
        print(f"Downloading {item['filename']}", flush=True)
        request = urllib.request.Request(item["url"], headers={"User-Agent": "sopwith-bootstrap"})
        partial = target.with_suffix(target.suffix + ".part")
        try:
            with urllib.request.urlopen(request, timeout=120) as response, partial.open("wb") as output:
                shutil.copyfileobj(response, output)
            if digest(partial) != item["sha256"]:
                raise RuntimeError(f"Checksum mismatch: {partial}")
            partial.replace(target)
        finally:
            partial.unlink(missing_ok=True)
    if digest(target) != item["sha256"]:
        raise RuntimeError(f"Checksum mismatch: {target}; remove it and retry")
    return target


def unpack_tar(item, destination, dirname):
    archive = download(item)
    destination.mkdir(parents=True, exist_ok=True)
    target = destination / dirname
    if not target.exists():
        with tempfile.TemporaryDirectory(dir=destination) as staging:
            with tarfile.open(archive) as package:
                package.extractall(staging, filter="data")
            (Path(staging) / dirname).rename(target)
    print(f"Available: {target}", flush=True)


def install_debs(group, destination):
    release = platform.freedesktop_os_release()
    if release.get("ID") != "ubuntu" or release.get("VERSION_ID") != "24.04":
        raise RuntimeError("Local dependency packages are for Ubuntu 24.04 only; see Documents/SETUP.md")
    if shutil.which("dpkg-deb") is None:
        raise RuntimeError("dpkg-deb is required to extract local dependencies")
    destination.mkdir(parents=True, exist_ok=True)
    for item in LOCK[group]:
        archive = download(item)
        marker = destination / ("." + item["filename"] + ".sha256")
        if not marker.exists() or marker.read_text().strip() != item["sha256"]:
            subprocess.run(["dpkg-deb", "-x", str(archive), str(destination)], check=True)
            marker.write_text(item["sha256"] + "\n")
    print(f"Available: {destination}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--local-toolchain", action="store_true", help="also install Ubuntu 24.04 ARM C tools")
    parser.add_argument("--local-simulator-libs", action="store_true", help="also install the tested Ubuntu 24.04 GUI library supplement")
    parser.add_argument("--local-desktop-libs", action="store_true", help="also install Ubuntu 24.04 SDL2 headers for the desktop reference (requires the SDL2 runtime)")
    parser.add_argument("--fetch-upstream", action="store_true", help="also fetch the pinned research source (not a game import)")
    parser.add_argument("--device-compatibility-sdk", action="store_true", help="also install SDK 2.2.0 for the connected older-OS test device")
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        parser.error("This bootstrap supports Linux x86_64; see Documents/SETUP.md for other hosts")
    if not hasattr(hashlib, "file_digest") or not hasattr(tarfile, "data_filter"):
        parser.error("Use Python 3.12+ (or a Python version with file_digest and tar data filters)")

    sdk = LOCK["sdk"]
    print("Playdate SDK license: https://play.date/dev/sdk-license/", flush=True)
    unpack_tar(sdk, ROOT / ".tools", "PlaydateSDK-" + sdk["version"])
    if args.device_compatibility_sdk:
        compatible = LOCK["device_compatibility_sdk"]
        unpack_tar(compatible, ROOT / ".tools", "PlaydateSDK-" + compatible["version"])
    if args.local_toolchain:
        destination = ROOT / ".tools/arm-toolchain"
        install_debs("arm_toolchain", destination)
        # Debian normally creates these using system-wide alternatives.
        # Recreate only the relocatable links inside this local installation.
        for name, target in (("include", "../../include/newlib"), ("lib", "newlib")):
            link = destination / "usr/lib/arm-none-eabi" / name
            if not os.path.lexists(link):
                link.symlink_to(target)
    if args.local_simulator_libs:
        install_debs("simulator_libraries", ROOT / ".tools/simulator-libs")
    if args.local_desktop_libs:
        install_debs("desktop_libraries", ROOT / ".tools/desktop-libs")
    if args.fetch_upstream:
        upstream = LOCK["upstream"]
        unpack_tar(upstream, ROOT / ".cache/research", "sdl-sopwith-" + upstream["revision"])
    print("Next: source tools/env.sh && bash tools/check-sdk.sh", flush=True)


if __name__ == "__main__":
    main()
