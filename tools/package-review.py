#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Zip the existing device bundle with review notes and a public source link."""
import argparse
import hashlib
import io
from pathlib import Path, PurePosixPath
import re
import subprocess
import urllib.error
import urllib.request
import zipfile

SOURCE_REPOSITORY = "https://github.com/Suvitruf/sopwith-playdate"


def metadata(path):
    return dict(line.split("=", 1) for line in path.read_text().splitlines() if "=" in line)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path,
                        default=root / "build/game-2.2.0/device/sopwith.pdx")
    parser.add_argument("--source-commit",
                        help="Verify and link a full commit SHA already published in the public repository")
    args = parser.parse_args()
    if args.source_commit is not None and not re.fullmatch(r"[0-9a-f]{40}", args.source_commit):
        parser.error("--source-commit must be a full, lowercase Git commit SHA")
    bundle = args.bundle.resolve()
    info = metadata(bundle / "pdxinfo")
    source_info = metadata(root / "Source/pdxinfo")
    for field in ("name", "bundleID", "version", "buildNumber"):
        if info.get(field) != source_info.get(field):
            parser.error(f"Built bundle differs from Source/pdxinfo: {field}")
    version, build = info["version"], info["buildNumber"]
    if not re.fullmatch(r"[0-9]+(?:\.[0-9]+){2}", version) or not build.isdigit():
        parser.error("Expected numeric version and buildNumber")
    if not (bundle / "pdex.bin").is_file() or not (bundle / "pdex.bin").stat().st_size:
        parser.error("The selected bundle has no native device executable")
    notes = (root / "Documents/REVIEWER_INSTRUCTIONS.txt").read_bytes()
    if f"Version: {version} (build {build})".encode() not in notes:
        parser.error("Update reviewer instructions for this build first")

    files = {"README_REVIEW.txt": notes,
             "COPYING.md": (root / "COPYING.md").read_bytes(),
             "AUTHORS.txt": (root / "Sources/core/AUTHORS").read_bytes()}
    for path in sorted(bundle.rglob("*")):
        if path.is_symlink():
            parser.error(f"Unexpected symlink in bundle: {path}")
        if path.is_file():
            files[f"sopwith.pdx/{path.relative_to(bundle).as_posix()}"] = path.read_bytes()
    for path in sorted((root / "LICENSES").rglob("*")):
        if path.is_file():
            files[path.relative_to(root).as_posix()] = path.read_bytes()

    # Compare code, build inputs, tests and licenses; repository presentation
    # may differ. Never include SDKs, saves, logs or .git.
    source_roots = {"Sources", "Source", "tools", "tests", "LICENSES"}
    root_files = {"CMakeLists.txt", "COPYING.md"}
    source_documents = {"Sources/README.md", "Sources/core/PORTING.md"}
    names = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=root)
    local_source = {}
    for name in sorted(set(names.decode().split("\0")) - {""}):
        relative = PurePosixPath(name)
        if relative.parts[0] not in source_roots and name not in root_files:
            continue
        if name in source_documents:
            continue
        path = root / name
        if path.is_symlink():
            parser.error(f"Unexpected source symlink: {name}")
        if path.is_file():
            local_source[name] = path.read_bytes()

    commit = args.source_commit
    if commit:
        # GitHub's public archive endpoint needs no account or credentials.
        request = urllib.request.Request(
            f"https://codeload.github.com/Suvitruf/sopwith-playdate/zip/{commit}",
            headers={"User-Agent": "sopwith-review-packager"})
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                source_zip = response.read()
            with zipfile.ZipFile(io.BytesIO(source_zip)) as archive:
                if archive.testzip() is not None:
                    parser.error("Public source archive failed its integrity check")
                prefix = f"sopwith-playdate-{commit}/"
                published_source = {}
                for entry in archive.infolist():
                    if entry.is_dir():
                        continue
                    if not entry.filename.startswith(prefix):
                        parser.error("Unexpected path in public source archive")
                    name = entry.filename[len(prefix):]
                    relative = PurePosixPath(name)
                    if relative.parts[0] not in source_roots and name not in root_files:
                        continue
                    if name in source_documents:
                        continue
                    if name in published_source:
                        parser.error(f"Duplicate public source path: {name}")
                    published_source[name] = archive.read(entry)
        except (urllib.error.URLError, TimeoutError, zipfile.BadZipFile) as error:
            parser.error(f"Cannot verify publicly downloadable source: {error}")
        differences = sorted(name for name in local_source.keys() | published_source.keys()
                             if local_source.get(name) != published_source.get(name))
        if differences:
            parser.error("Public source differs from this checkout: " + ", ".join(differences))
        revision_info = (
            f"Source commit: {commit}\n"
            f"Browse this version: {SOURCE_REPOSITORY}/tree/{commit}\n"
            f"Download this version: {SOURCE_REPOSITORY}/archive/{commit}.zip\n\n"
            "Source code, build tools, tests and license files at this revision "
            "were downloaded without credentials and matched this checkout.\n"
        )
        setup_url = f"{SOURCE_REPOSITORY}/blob/{commit}/Documents/SETUP.md"
    else:
        revision_info = (
            "No exact source revision is pinned or verified in this ZIP. "
            "The repository URL identifies the project's source location; "
            "publication of the matching source is still pending.\n"
        )
        setup_url = "Documents/SETUP.md in the source repository"

    files["SOURCE_CODE.txt"] = (
        f"SOPWITH FOR PLAYDATE - CORRESPONDING SOURCE\n"
        f"Version: {version} (build {build})\n"
        f"Bundle ID: {info['bundleID']}\n\n"
        f"Public repository: {SOURCE_REPOSITORY}\n"
        f"{revision_info}\n"
        "Source files are not bundled in the game ZIP.\n\n"
        f"Build instructions: {setup_url}\n"
        "SDK/toolchain versions and downloads: tools/dependencies.json\n"
        "Upstream revision and modifications: Sources/core/PORTING.md\n"
        "\n"
        "The Playdate SDK is an external build dependency and is not included "
        "in the source repository. License notices accompany the game; source "
        "files retain their individual copyright and license notices.\n"
    ).encode()

    files["PACKAGE_MANIFEST.sha256"] = "".join(
        f"{hashlib.sha256(data).hexdigest()}  {name}\n" for name, data in sorted(files.items())
    ).encode()
    output_dir = root / "build/releases"
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / f"sopwith-playdate-{version}-build{build}.zip"
    temporary = output.with_suffix(".zip.tmp")
    with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            # Fixed archive metadata yields identical ZIPs for identical inputs.
            entry = zipfile.ZipInfo(name, date_time=(2000, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = (0o100755 if name.endswith(".sh") else 0o100644) << 16
            archive.writestr(entry, data, compresslevel=9)
    with zipfile.ZipFile(temporary) as archive:
        if archive.testzip() is not None:
            raise RuntimeError("ZIP integrity check failed")
        for name, data in files.items():
            if archive.read(name) != data:
                raise RuntimeError(f"Archive content mismatch: {name}")
    temporary.replace(output)
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix(".zip.sha256").write_text(f"{digest}  {output.name}\n")
    print(output)
    print(f"{len(files)} files, {output.stat().st_size:,} bytes; SHA-256 {digest}")
    if commit:
        print(f"Verified {len(local_source)} public source files at {commit}")
    else:
        print("Included repository URL; matching source publication is pending (not verified).")


if __name__ == "__main__":
    main()
