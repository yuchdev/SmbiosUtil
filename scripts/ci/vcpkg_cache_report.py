#!/usr/bin/env python3
"""vcpkg binary-cache health line and cache key scheme (milestone 0014, Story 02.0 Task 02).

Three subcommands:

``snapshot``
    Before configure: count the archives in the cache directory and keep the
    count in ``$CI_TEMP_DIR/vcpkg-cache-before.json`` (mode 0600, never through
    a symlink). Prints nothing.

``report --log <configure log>``
    After configure: print exactly one line::

        info: vcpkg cache <exact|partial|miss>, <n> archives, <MB> MB, <k> packages built from source

    ``<n>``/``<MB>`` are the archives in the cache directory after configure.
    ``<k>`` comes from a tolerant parse of vcpkg's own "Restored <n>
    package(s) from" and "Building <port>:<triplet>" lines; if the log cannot
    be read or has none of vcpkg's markers the tail is
    ``built-from-source unknown``. State: ``miss`` when the cache was empty
    before configure or nothing was restored while something was built;
    ``exact`` when nothing was built from source; ``partial`` otherwise
    (also whenever the count is unknown but the cache was not empty).
    Always exits 0 (informational).

``key --flavor <default|asan|gui> --runner-label <label>``
    Print the cache key and restore prefix (one ``info:`` line) for Story
    03.0's ``actions/cache`` steps:

    - key: ``vcpkg-<flavor>-<runner label>-<cxx id+major>-<hash16>-<run_id>``
      where ``<hash16>`` hashes ``vcpkg.json``, ``vcpkg-configuration.json``
      and ``scripts/ci/requirements-tools.txt``. The compiler is part of the
      key (plan E2); the image version deliberately is not (weekly churn).
    - restore prefix: the same key without ``-<run_id>``.

Cache rules (threat model CR-2), shared with ``bootstrap_vcpkg.py`` through
:func:`save_allowed`: a save happens only on a ``push`` to ``refs/heads/master``
or a ``schedule`` run there; every other run restores only, and tag-event
packaging legs use no cache at all.

Reads only neutral names (``CI_TEMP_DIR``, ``CI_RUN_ID``, ``CI_WORKSPACE``).
Python 3 standard library only.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import argparse  # noqa: E402
import hashlib  # noqa: E402
import json  # noqa: E402
import re  # noqa: E402
import stat  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import Final, Optional, Sequence, Tuple  # noqa: E402

import ci_step  # noqa: E402
import env_names  # noqa: E402
from reporters import get_reporter  # noqa: E402

#: Default ``files`` binary-cache directory name under ``$CI_TEMP_DIR``.
CACHE_DIR_NAME: Final = "vcpkg-binary-cache"
#: Snapshot hand-off file under ``$CI_TEMP_DIR``.
SNAPSHOT_FILE: Final = "vcpkg-cache-before.json"
#: Cache flavors (one cache line per build flavor).
FLAVORS: Final = ("default", "asan", "gui")
#: Files whose content decides the cache key's hash (repo-relative).
KEY_FILES: Final = ("vcpkg.json", "vcpkg-configuration.json", "scripts/ci/requirements-tools.txt")
#: The only ref whose push/schedule runs may write the cache (CR-2).
TRUSTED_REF: Final = "refs/heads/master"
#: Lines of a configure log read at most.
MAX_LOG_LINES: Final = 500_000

_TOKEN = re.compile(r"^[0-9A-Za-z._-]{1,40}$")
_RESTORED = re.compile(r"Restored (\d+) package")
_BUILDING = re.compile(r"^\s*(?:-- )?Building ([A-Za-z0-9._-]+:[A-Za-z0-9._-]+)")
_MARKERS = re.compile(r"All requested (?:installations completed successfully|packages are "
                      r"currently installed)|The following packages (?:will be built and "
                      r"installed|are already installed)|Restored \d+ package|"
                      r"Installing \d+/\d+ ")


def save_allowed(event: str, ref: str) -> bool:
    """Return whether this run may write the vcpkg cache (``push``/``schedule`` on master only)."""
    return event in ("push", "schedule") and ref == TRUSTED_REF


def count_archives(cache_dir: Path) -> Tuple[int, int]:
    """Return ``(files, bytes)`` of the ``*.zip`` archives under ``cache_dir``.

    A missing directory is ``(0, 0)``. Symlinks are neither followed nor counted.
    """
    files = total = 0
    if not cache_dir.is_dir() or cache_dir.is_symlink():
        return 0, 0
    for root, _dirs, names in os.walk(cache_dir, followlinks=False):
        for name in names:
            if not name.endswith(".zip"):
                continue
            try:
                st = os.lstat(os.path.join(root, name))
            except OSError:
                continue
            if stat.S_ISREG(st.st_mode):  # lstat: a symlink is never S_ISREG
                files += 1
                total += st.st_size
    return files, total


def parse_log(log: Path) -> Tuple[Optional[int], Optional[int]]:
    """Return ``(restored, built)`` from a configure log; ``(None, None)`` if unparseable."""
    restored = 0
    built = set()
    marker = False
    try:
        with open(log, "r", encoding="utf-8", errors="replace") as fh:
            for number, line in enumerate(fh):
                if number >= MAX_LOG_LINES:
                    break
                line = line[:4096]
                if _MARKERS.search(line):
                    marker = True
                match = _RESTORED.search(line)
                if match:
                    restored += int(match.group(1))
                match = _BUILDING.match(line)
                if match:
                    built.add(match.group(1))
                    marker = True
    except OSError:
        return None, None
    if not marker:
        return None, None
    return restored, len(built)


def classify(before_files: int, restored: Optional[int], built: Optional[int]) -> str:
    """Return ``exact``, ``partial`` or ``miss`` (see the module docstring)."""
    if before_files == 0:
        return "miss"
    if built is None or restored is None:
        return "partial"
    if built == 0:
        return "exact"
    return "partial" if restored > 0 else "miss"


def report_line(cache_dir: Path, log: Path, before_files: int) -> str:
    """Return the single ``info: vcpkg cache ...`` line."""
    files, size = count_archives(cache_dir)
    restored, built = parse_log(log)
    state = classify(before_files, restored, built)
    tail = "built-from-source unknown" if built is None else f"{built} packages built from source"
    return f"info: vcpkg cache {state}, {files} archives, {size / 1048576:.1f} MB, {tail}"


def cxx_tag(cxx_id: str, version: str) -> str:
    """Return ``<id><major>`` (``gcc13``, ``appleclang17``, ``vs202217``) or ``unknown``."""
    major = version.split(".", 1)[0]
    tag = f"{cxx_id}{major}" if major.isdigit() else cxx_id
    return tag if _TOKEN.match(tag) and cxx_id not in ("-", "?") else "unknown"


def key_hash(repo_root: Path) -> str:
    """Return the first 16 hex characters of the SHA-256 over :data:`KEY_FILES`.

    Raises:
        OSError: A key file is missing.
    """
    digest = hashlib.sha256()
    for rel in KEY_FILES:
        digest.update(rel.encode("ascii") + b"\0")
        digest.update((repo_root / rel).read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()[:16]


def cache_key(flavor: str, runner_label: str, cxx: str, repo_root: Path,
              run_id: str) -> Tuple[str, str]:
    """Return ``(key, restore_prefix)`` for the vcpkg binary cache.

    Raises:
        ValueError: ``flavor`` is not in :data:`FLAVORS`, or a token fails the allowlist.
    """
    if flavor not in FLAVORS:
        raise ValueError(f"flavor must be one of {', '.join(FLAVORS)}")
    for value in (runner_label, cxx, run_id):
        if not _TOKEN.match(value):
            raise ValueError("runner label, compiler tag and run id must match [0-9A-Za-z._-]")
    prefix = f"vcpkg-{flavor}-{runner_label}-{cxx}-{key_hash(repo_root)}"
    return f"{prefix}-{run_id}", prefix


def _snapshot_path() -> Path:
    return Path(env_names.get("CI_TEMP_DIR", required=True)) / SNAPSHOT_FILE


def write_snapshot(cache_dir: Path, path: Path) -> None:
    """Write the before-configure count to ``path``: mode 0600, never through a symlink."""
    if path.is_symlink():
        raise OSError("refusing symlink snapshot file")
    files, size = count_archives(cache_dir)
    flags = (os.O_WRONLY | os.O_CREAT | os.O_TRUNC
             | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_BINARY", 0))
    fd = os.open(str(path), flags, 0o600)
    try:
        os.write(fd, json.dumps({"files": files, "bytes": size}).encode("ascii"))
    finally:
        os.close(fd)


def read_snapshot(path: Path) -> Optional[int]:
    """Return the snapshot's file count, or ``None`` if there is no valid snapshot."""
    try:
        data = json.loads(path.read_text(encoding="ascii"))
    except (OSError, ValueError):
        return None
    files = data.get("files") if isinstance(data, dict) else None
    return files if isinstance(files, int) and files >= 0 else None


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="vcpkg_cache_report.py",
                                     description="vcpkg binary-cache health and key.")
    parser.add_argument("--reporter", default=None)
    parser.add_argument("--cache-dir", default=None,
                        help=f"default $CI_TEMP_DIR/{CACHE_DIR_NAME}")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("snapshot", help="count archives before configure")
    report = sub.add_parser("report", help="print the cache health line after configure")
    report.add_argument("--log", required=True, help="configure log")
    key = sub.add_parser("key", help="print the cache key and restore prefix")
    key.add_argument("--flavor", required=True, choices=FLAVORS)
    key.add_argument("--runner-label", required=True)
    key.add_argument("--cxx", default=None, help="<id><major>; default: probed")
    return parser


def main(argv: Sequence[str]) -> int:
    """Run one subcommand; always returns 0 (informational step)."""
    try:
        reporter = get_reporter(None)
    except ValueError:
        reporter = get_reporter("plain")
    try:
        args = _parser().parse_args(list(argv))
        if args.reporter:
            reporter = get_reporter(args.reporter)
        cache_dir = (Path(args.cache_dir) if args.cache_dir
                     else Path(env_names.get("CI_TEMP_DIR", required=True)) / CACHE_DIR_NAME)
        if args.command == "snapshot":
            write_snapshot(cache_dir, _snapshot_path())
            return 0
        if args.command == "report":
            before = read_snapshot(_snapshot_path())
            if before is None:  # no snapshot: assume configure wrote nothing new
                before = count_archives(cache_dir)[0]
            ci_step._emit(reporter.info(report_line(cache_dir, Path(args.log), before)))
            return 0
        cxx = args.cxx
        if cxx is None:
            import print_env  # lazy: probes the compiler
            cxx = cxx_tag(*print_env.probe_cxx())
        key, prefix = cache_key(args.flavor, args.runner_label, cxx,
                                Path(env_names.get("CI_WORKSPACE", required=True)),
                                env_names.get("CI_RUN_ID", required=True))
        ci_step._emit(reporter.info(f"info: vcpkg cache key: {key}, restore prefix: {prefix}"))
        return 0
    except SystemExit:
        ci_step._emit(reporter.info("info: vcpkg cache report unavailable (usage)"))
        return 0
    except Exception as exc:  # noqa: BLE001 - informational step: one line, exit 0
        ci_step._emit(reporter.info(f"info: vcpkg cache report unavailable ({type(exc).__name__})"))
        return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
