"""Locate a vcpkg root (the value ``VCPKG_ROOT`` should have) on this machine.

vcpkg has no fixed install location, so this script checks, in order:

1. the ``VCPKG_ROOT`` / ``VCPKG_INSTALLATION_ROOT`` environment variables
   (the latter is what GitHub Actions runners set);
2. a ``vcpkg`` executable on ``PATH`` (its symlink-resolved parent directory);
3. Windows only: ``%LOCALAPPDATA%\\vcpkg\\vcpkg.path.txt`` (written by
   ``vcpkg integrate install``) and the vcpkg bundled with Visual Studio;
4. common clone locations (``~/vcpkg``, ``C:\\vcpkg``, ``/usr/local/share/vcpkg``, ...);
5. a shallow, bounded scan of the home directory for a ``.vcpkg-root`` marker.

A directory counts as a vcpkg root only if it contains the ``.vcpkg-root``
marker file or ``scripts/buildsystems/vcpkg.cmake`` (the toolchain file this
project's ``CMakePresets.json`` points at).

Usage: ``python scripts/find_vcpkg_root.py``. Prints every root found (first
one is the best candidate) plus an ``export`` hint, and exits 0; prints a
message to stderr and exits 1 if none is found. Read-only: it never modifies
anything. Standard library only.
"""

from __future__ import annotations

import glob
import os
import shutil
import sys
from pathlib import Path

# How deep the home-directory scan descends; vcpkg is normally cloned near the top.
_SCAN_MAX_DEPTH = 3
# Directories never worth descending into during the scan.
_SCAN_SKIP = {"node_modules", ".git", ".cache", "Library", "AppData", ".cargo", ".rustup"}


def is_vcpkg_root(path: Path) -> bool:
    """True if ``path`` looks like a vcpkg root directory."""
    try:
        return path.is_dir() and (
            (path / ".vcpkg-root").is_file()
            or (path / "scripts" / "buildsystems" / "vcpkg.cmake").is_file()
        )
    except OSError:
        return False


def _from_env():
    for var in ("VCPKG_ROOT", "VCPKG_INSTALLATION_ROOT"):
        value = os.environ.get(var)
        if value:
            yield f"${var}", Path(value)


def _from_path():
    exe = shutil.which("vcpkg")
    if exe:
        yield "vcpkg on PATH", Path(exe).resolve().parent


def _from_windows():
    if os.name != "nt":
        return
    local = os.environ.get("LOCALAPPDATA")
    if local:
        path_file = Path(local) / "vcpkg" / "vcpkg.path.txt"
        try:
            text = path_file.read_text(encoding="utf-8").strip()
        except OSError:
            text = ""
        if text:
            yield "vcpkg integrate install (vcpkg.path.txt)", Path(text)
    for base in (os.environ.get("ProgramFiles"), os.environ.get("ProgramFiles(x86)")):
        if base:
            pattern = str(Path(base) / "Microsoft Visual Studio" / "*" / "*" / "VC" / "vcpkg")
            for hit in sorted(glob.glob(pattern)):
                yield "Visual Studio bundled vcpkg", Path(hit)


def _from_common_locations():
    home = Path.home()
    candidates = [
        home / "vcpkg",
        home / "dev" / "vcpkg",
        home / "src" / "vcpkg",
        home / ".vcpkg",
        Path("/usr/local/share/vcpkg"),
        Path("/opt/vcpkg"),
        Path("/opt/homebrew/share/vcpkg"),
        Path("C:/vcpkg"),
        Path("C:/src/vcpkg"),
        Path("C:/dev/vcpkg"),
    ]
    for candidate in candidates:
        yield "common location", candidate


def _scan_home():
    """Bounded walk of $HOME looking for a ``.vcpkg-root`` marker file."""
    home = Path.home()
    base_depth = len(home.parts)
    for dirpath, dirnames, filenames in os.walk(home):
        depth = len(Path(dirpath).parts) - base_depth
        if ".vcpkg-root" in filenames:
            yield "scan of home directory", Path(dirpath)
        if depth >= _SCAN_MAX_DEPTH:
            dirnames[:] = []
        else:
            dirnames[:] = [d for d in dirnames if d not in _SCAN_SKIP]


def find_roots():
    """Return ``[(source, resolved_root), ...]``, de-duplicated, best candidate first."""
    seen = set()
    found = []
    for source_fn in (_from_env, _from_path, _from_windows, _from_common_locations, _scan_home):
        for source, path in source_fn():
            try:
                resolved = path.resolve()
            except OSError:
                continue
            if resolved in seen or not is_vcpkg_root(resolved):
                continue
            seen.add(resolved)
            found.append((source, resolved))
    return found


def main() -> int:
    roots = find_roots()
    if not roots:
        print(
            "No vcpkg root found. Install one with:\n"
            "  git clone https://github.com/microsoft/vcpkg ~/vcpkg\n"
            "  ~/vcpkg/bootstrap-vcpkg.sh    (bootstrap-vcpkg.bat on Windows)\n"
            "Use a full clone, not --depth 1: vcpkg.json pins a builtin-baseline commit.",
            file=sys.stderr,
        )
        return 1

    for source, root in roots:
        print(f"Found vcpkg root: {root}  [{source}]")

    best = roots[0][1]
    print()
    if os.name == "nt":
        print(f'Set it with:  set VCPKG_ROOT={best}   (PowerShell: $env:VCPKG_ROOT = "{best}")')
    else:
        print(f"Set it with:  export VCPKG_ROOT={best}")
    print(f"Toolchain file: {best / 'scripts' / 'buildsystems' / 'vcpkg.cmake'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
