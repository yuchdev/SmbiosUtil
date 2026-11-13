#!/usr/bin/env python3
"""Download, verify and unpack one pinned actionlint binary (milestone 0014, Story 02.0 Task 03).

The release asset for this host is downloaded once into a cache directory,
its SHA-256 is checked against :data:`ASSETS` (taken from the release's own
``checksums.txt``, which matched the release API's published digests on
2026-11-13), and only the single ``actionlint`` (``actionlint.exe``) member is
extracted, into a fresh ``mkdtemp`` directory under ``$CI_TEMP_DIR`` (threat
model CR-7). Before extraction every archive member is checked: an absolute
name, a drive letter, a ``..`` component, a link or a device makes the whole
archive refused. ``extractall`` is never called. A cached archive is re-hashed
on every use.

Usage::

    python3 scripts/ci/install_actionlint.py                 # ok: line + shellcheck info: line
    python3 scripts/ci/install_actionlint.py --run -- FILE... # install, then run actionlint FILE...

``--run`` is what ``lint.py`` calls through ``ci_step``; actionlint's own
output then goes to the step log, not to the console. Exit codes: 0 success,
1 refused archive or hash mismatch, 2 unsupported host or download failure,
otherwise actionlint's own exit code under ``--run``. Python 3.9+ stdlib only.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import platform
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath
from typing import Callable, Dict, Optional, Sequence, Tuple

_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

import ci_step  # noqa: E402
import env_names  # noqa: E402
from reporters import get_reporter  # noqa: E402

REPO_ROOT = _HERE.parents[1]
VERSION = "1.7.12"
LABEL = "actionlint"
URL = "https://github.com/rhysd/actionlint/releases/download/v{version}/{asset}"
#: (os, arch) -> (asset name, sha256), from the v1.7.12 release's checksums.txt.
ASSETS: Dict[Tuple[str, str], Tuple[str, str]] = {
    ("darwin", "amd64"): ("actionlint_1.7.12_darwin_amd64.tar.gz",
                          "5b44c3bc2255115c9b69e30efc0fecdf498fdb63c5d58e17084fd5f16324c644"),
    ("darwin", "arm64"): ("actionlint_1.7.12_darwin_arm64.tar.gz",
                          "aba9ced2dee8d27fecca3dc7feb1a7f9a52caefa1eb46f3271ea66b6e0e6953f"),
    ("linux", "amd64"): ("actionlint_1.7.12_linux_amd64.tar.gz",
                         "8aca8db96f1b94770f1b0d72b6dddcb1ebb8123cb3712530b08cc387b349a3d8"),
    ("linux", "arm64"): ("actionlint_1.7.12_linux_arm64.tar.gz",
                         "325e971b6ba9bfa504672e29be93c24981eeb1c07576d730e9f7c8805afff0c6"),
    ("windows", "amd64"): ("actionlint_1.7.12_windows_amd64.zip",
                           "6e7241b51e6817ea6a047693d8e6fed13b31819c9a0dd6c5a726e1592d22f6e9"),
    ("windows", "arm64"): ("actionlint_1.7.12_windows_arm64.zip",
                           "cadcf7ea4efe3a68728893813643cebe1185e5b1d4be5b96245f65c9a4d5ea41"),
}
#: Default archive cache (``build/`` is git-ignored).
DEFAULT_CACHE_DIR = REPO_ROOT / "build" / "ci-tools"
DOWNLOAD_TIMEOUT_S = 30.0
MAX_ARCHIVE_BYTES = 64 * 1024 * 1024
MAX_BINARY_BYTES = 128 * 1024 * 1024
BINARY_NAMES = ("actionlint", "actionlint.exe")

EXIT_REFUSED = 1
EXIT_ENV = 2


class InstallError(Exception):
    """Installation failed; ``exit_code`` is 1 (refused/hash) or 2 (host/network)."""

    def __init__(self, message: str, exit_code: int = EXIT_REFUSED) -> None:
        super().__init__(message)
        self.exit_code = exit_code


def host_key(system: Optional[str] = None, machine: Optional[str] = None) -> Tuple[str, str]:
    """Return the ``(os, arch)`` key of :data:`ASSETS` for this host."""
    system = (system or platform.system()).lower()
    machine = (machine or platform.machine()).lower()
    arch = {"x86_64": "amd64", "amd64": "amd64", "arm64": "arm64", "aarch64": "arm64"}.get(
        machine, machine)
    return system, arch


def sha256_file(path: Path) -> str:
    """Return the hex SHA-256 of ``path``."""
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url: str, dest: Path, timeout_s: float = DOWNLOAD_TIMEOUT_S) -> None:
    """Download ``url`` to ``dest`` with one retry and a size cap.

    Raises:
        InstallError: (exit 2) the download failed twice or exceeded the cap.
    """
    last = ""
    for _ in range(2):  # at most one retry (Story 02.0 constraint)
        try:
            with urllib.request.urlopen(url, timeout=timeout_s) as resp, dest.open("wb") as out:
                total = 0
                for chunk in iter(lambda: resp.read(65536), b""):
                    total += len(chunk)
                    if total > MAX_ARCHIVE_BYTES:
                        raise InstallError("archive larger than the size cap", EXIT_ENV)
                    out.write(chunk)
            return
        except InstallError:
            raise
        except OSError as exc:
            last = type(exc).__name__
            time.sleep(1)
    raise InstallError(f"download failed ({last})", EXIT_ENV)


def _check_name(name: str) -> None:
    if not name or name.startswith(("/", "\\")) or (len(name) > 1 and name[1] == ":") \
            or "\\" in name:
        raise InstallError(f"archive member with an absolute or non-POSIX name: {name[:60]!r}")
    if ".." in PurePosixPath(name).parts:
        raise InstallError(f"archive member with a '..' component: {name[:60]!r}")


def _top_name(name: str) -> str:
    parts = [p for p in PurePosixPath(name).parts if p != "."]
    return parts[0] if len(parts) == 1 else ""


def _write_binary(src, dest_dir: Path, name: str) -> Path:
    target = dest_dir / name
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_BINARY", 0)
    fd = os.open(str(target), flags, 0o700)
    total = 0
    with os.fdopen(fd, "wb") as out:
        for chunk in iter(lambda: src.read(65536), b""):
            total += len(chunk)
            if total > MAX_BINARY_BYTES:
                raise InstallError("actionlint member larger than the size cap")
            out.write(chunk)
    target.chmod(stat.S_IRWXU)
    return target


def extract_binary(archive: Path, dest_dir: Path) -> Path:
    """Validate every member of ``archive``, then extract only the actionlint binary.

    Raises:
        InstallError: (exit 1) an unsafe member, or no top-level actionlint member.
    """
    if archive.name.endswith(".zip"):
        with zipfile.ZipFile(archive) as zf:
            infos = zf.infolist()
            for info in infos:
                _check_name(info.filename)
                mode = info.external_attr >> 16
                if stat.S_ISLNK(mode):
                    raise InstallError(f"archive member is a link: {info.filename[:60]!r}")
                if stat.S_ISCHR(mode) or stat.S_ISBLK(mode) or stat.S_ISFIFO(mode):
                    raise InstallError(f"archive member is a device: {info.filename[:60]!r}")
            for info in infos:
                if not info.is_dir() and _top_name(info.filename) in BINARY_NAMES:
                    with zf.open(info) as src:
                        return _write_binary(src, dest_dir, _top_name(info.filename))
    else:
        with tarfile.open(archive, "r:gz") as tf:
            members = tf.getmembers()
            for member in members:
                _check_name(member.name)
                if member.issym() or member.islnk():
                    raise InstallError(f"archive member is a link: {member.name[:60]!r}")
                if member.isdev():
                    raise InstallError(f"archive member is a device: {member.name[:60]!r}")
                if not (member.isfile() or member.isdir()):
                    raise InstallError(f"archive member of unknown type: {member.name[:60]!r}")
            for member in members:
                if member.isfile() and _top_name(member.name) in BINARY_NAMES:
                    src = tf.extractfile(member)
                    if src is None:
                        break
                    with src:
                        return _write_binary(src, dest_dir, _top_name(member.name))
    raise InstallError("no top-level actionlint member in the archive")


Fetch = Callable[[str, Path], None]


def install(cache_dir: Path, temp_dir: Path, key: Optional[Tuple[str, str]] = None,
            fetch: Fetch = download,
            assets: Optional[Dict[Tuple[str, str], Tuple[str, str]]] = None) -> Path:
    """Return the path of a verified actionlint binary in a fresh directory under ``temp_dir``.

    Raises:
        InstallError: Unsupported host, download failure, hash mismatch or unsafe archive.
    """
    assets = ASSETS if assets is None else assets
    key = key or host_key()
    if key not in assets:
        raise InstallError(f"no pinned actionlint asset for {key[0]}/{key[1]}", EXIT_ENV)
    asset, want = assets[key]
    cache_dir.mkdir(parents=True, exist_ok=True)
    archive = cache_dir / asset
    if not (archive.is_file() and sha256_file(archive) == want):
        partial = cache_dir / (asset + ".part")
        try:
            fetch(URL.format(version=VERSION, asset=asset), partial)
            got = sha256_file(partial)
            if got != want:
                raise InstallError(f"sha256: expected {want[:12]} -> actual {got[:12]}")
            os.replace(str(partial), str(archive))
        finally:
            if partial.exists():
                partial.unlink()
    temp_dir.mkdir(parents=True, exist_ok=True)
    dest = Path(tempfile.mkdtemp(prefix="actionlint-", dir=str(temp_dir)))
    try:
        return extract_binary(archive, dest)
    except BaseException:
        shutil.rmtree(dest, ignore_errors=True)
        raise


def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="install_actionlint.py",
                                description="Install the pinned actionlint binary.")
    p.add_argument("--cache-dir", type=Path, default=DEFAULT_CACHE_DIR)
    p.add_argument("--run", action="store_true", help="run actionlint on the files after '--'")
    p.add_argument("--reporter", default=None)
    p.add_argument("files", nargs="*")
    return p


def main(argv: Sequence[str]) -> int:
    """Install (and optionally run) actionlint; return the exit code."""
    args = _parser().parse_args(list(argv))
    try:
        reporter = get_reporter(args.reporter)
    except ValueError:
        reporter = get_reporter("plain")
    start = time.monotonic()
    temp_dir = Path(env_names.get("CI_TEMP_DIR", required=True))
    try:
        tool = install(args.cache_dir, temp_dir)
    except InstallError as exc:
        ci_step._emit(reporter.info(f"error: actionlint install: {exc}"))
        return exc.exit_code
    shellcheck = "yes" if shutil.which("shellcheck") else "no"
    if args.run:
        ci_step._emit(reporter.info(f"info: actionlint {VERSION}, shellcheck on PATH: {shellcheck}"))
        proc = subprocess.run([str(tool), *args.files], stdin=subprocess.DEVNULL)
        return proc.returncode
    elapsed = round(time.monotonic() - start, 1)
    ci_step._emit(reporter.step_ok(LABEL, f"{VERSION} at {tool}", elapsed)
                  + reporter.info(f"info: shellcheck on PATH: {shellcheck}"))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step._emit([f"error: install_actionlint.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
