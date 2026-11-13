#!/usr/bin/env python3
"""The GUI job's binary search and its two merge-blocking guards (milestone 0014, Story 03.0 Task 03).

Ports the inline bash of milestone 0011's ``gui-build`` job (old ``ci.yml``,
inventory C17, C18, C20) with the same, fail-closed semantics, so the proofs
of ``docs/security/2026-11-11-gui-test-tiers.md`` survive the CI rewrite:

``find-binary --build-dir build/<preset>`` (C17)
    Walk the build tree (symlinks neither followed nor returned) for a
    **regular file** named ``smbios_gui`` or ``smbios_gui.exe`` whose path
    matches ``*/smbios_gui/bin/*`` (``find -type f -path ... -name ...``;
    ``*`` crosses ``/``, so ``smbios_gui/bin/Debug/smbios_gui.exe`` counts).
    The first match in sorted walk order is reported:
    ``ok: smbios_gui: <path> (<bytes> bytes)``. None found:
    ``smbios_gui binary: expected 1 -> actual 0``. A directory of the tree
    that cannot be listed fails closed with
    ``smbios_gui binary search failed (<errno name>)`` (review S3-4; the old
    ``|| true`` ignored it, so a second binary could hide there).
``hook-strings --binary <path>`` (R-7, C18)
    Count the non-overlapping occurrences of each of :data:`HOOK_PATTERNS` in
    the shipped binary, reading it in chunks with a ``len(pattern)-1``
    overlap. Any count above 0 fails with
    ``test hook present in shipped smbios_gui: expected 0 -> <n>``. A path
    that is not a regular file (a symlink, a directory, a FIFO, missing) or
    any read error fails closed with
    ``hook string check could not read the shipped smbios_gui (<errno name>)``.
    The old ``grep -c`` counted *lines* with a match; this counts
    *occurrences*. For a "must be 0" gate the two agree: no line has a match
    if and only if there is no occurrence. A binary smaller than
    :data:`MIN_BINARY_BYTES` (empty or truncated, so a 0 proves nothing)
    fails with ``smbios_gui binary size: expected >= <floor> -> <n>``
    (review S3-2; stronger than the old ``grep -c``).
``local-tier-absent --build-dir <dir> [--config Debug]`` (R-8, C20)
    All three checks run and all must pass:

    1. ``ctest --test-dir <dir> [-C <config>] -N -L gui-local`` exits 0 and
       prints exactly one ``Total Tests: <n>`` line (CR stripped, surrounding
       whitespace tolerated) with ``n == 0``;
    2. the same for ``-N -R smbios_gui_local_test|smbios_gui_autoexit``;
    3. a walk of the build tree finds no entry - file, directory or symlink
       (not followed), the same as ``find`` without ``-type f`` - whose name
       starts with ``smbios_gui_local_test`` or ``smbios_gui_autoexit``,
       compared with ``casefold()``. Up to 5 relative paths are printed;
    4. the directory is a configured CTest build tree (review S3-3; ctest
       prints ``Total Tests: 0`` and exits 0 for *any* directory, so checks 1
       and 2 alone pass on a wrong ``--build-dir``):
       ``<dir>/CTestTestfile.cmake`` is a regular file (not a symlink), and
       ``ctest --test-dir <dir> [-C <config>] -N -L gui-ci`` exits 0 with
       exactly one ``Total Tests: <n>`` line and ``n > 0``.

    A missing ``Total Tests:`` line, more than one, a non-zero ctest exit, a
    ctest that cannot start or times out, or a walk error is a failure, never
    a 0 (threat model CR-12).

Output follows RC1: one ``ok:`` line, or one failure line plus discrepancy
lines, through the ``$CI_REPORTER`` backend (``ci_step.emit``, P11), and one
RC3 record per check. Printed build-tree paths pass an allowlist, else
``<path>`` (CR-4). ``smbios_gui`` itself is never executed.

Exit codes: 0 pass, 1 a check failed, 2 usage or environment error. Python
3.9+ standard library only.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import argparse  # noqa: E402
import errno  # noqa: E402
import fnmatch  # noqa: E402
import re  # noqa: E402
import stat  # noqa: E402
import subprocess  # noqa: E402
import time  # noqa: E402
from dataclasses import dataclass, field  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import Final, List, Optional, Sequence, Tuple  # noqa: E402

import ci_step  # noqa: E402
import env_names  # noqa: E402
from reporters import Reporter, child_environment, get_reporter  # noqa: E402

#: R-7: the test-only auto-exit hook's strings; the shipped binary holds neither.
HOOK_PATTERNS: Final = (b"--exit-after-frames", b"usage: --exit-after-frames")
#: C17: names of the shipped GUI binary, and the ``find -path`` pattern.
BINARY_NAMES: Final = ("smbios_gui", "smbios_gui.exe")
BINARY_PATH_PATTERN: Final = "*/smbios_gui/bin/*"
#: R-8: name prefixes of every local-only GUI artifact (test target and twin).
LOCAL_PREFIXES: Final = ("smbios_gui_local_test", "smbios_gui_autoexit")
#: R-8: the two ctest selections (label, and name in case a label is missing).
LOCAL_LABEL_ARGS: Final = ("-L", "gui-local")
LOCAL_NAME_ARGS: Final = ("-R", "smbios_gui_local_test|smbios_gui_autoexit")
#: R-8 (review S3-3): proof that ``--build-dir`` is a configured GUI build tree.
CTEST_FILE: Final = "CTestTestfile.cmake"
CONFIGURED_ARGS: Final = ("-L", "gui-ci")
#: R-7 (review S3-2): a shipped Dear ImGui/GLFW binary is megabytes; anything
#: below this floor is empty or truncated, and a 0 count there proves nothing.
MIN_BINARY_BYTES: Final = 64 * 1024
#: Paths printed on a local-artifact failure (R-8).
MAX_SHOWN_PATHS: Final = 5
#: ``ctest -N`` only lists; it finishes in well under a second.
CTEST_TIMEOUT_S: Final = 60.0
#: Read size of the hook-string scan.
CHUNK: Final = 1 << 20

#: Labels of the RC1 lines and RC3 records.
LABEL_BINARY: Final = "smbios_gui"
LABEL_HOOKS: Final = "gui hook strings"
OK_HOOKS: Final = "no test hook strings in the shipped smbios_gui"
LABEL_LOCAL: Final = "gui-local tier"
OK_LOCAL: Final = "no gui-local tests configured"

#: CR-12: exactly one such line, after CR stripping; ``[0-9]``, never ``\\d``.
_TOTAL_TESTS: Final = re.compile(rb"^\s*Total Tests:\s*([0-9]{1,9})\s*$")
_PATH_TOKEN: Final = re.compile(r"^[A-Za-z0-9._/+-]{1,160}$")
_CONFIG: Final = re.compile(r"^[A-Za-z0-9_]{1,32}$")

EXIT_OK: Final = 0
EXIT_GATE: Final = 1
EXIT_USAGE: Final = env_names.EXIT_ENV_ERROR


@dataclass
class Ctx:
    """The backend that renders this script's lines, and the RC3 records file."""

    reporter: Reporter
    results: Path


def default_ctx(reporter: Optional[Reporter] = None) -> Ctx:
    """Return a :class:`Ctx` from the neutral names (``$CI_REPORTER``, ``$CI_RESULTS_FILE``)."""
    return Ctx(reporter or get_reporter(None),
               Path(env_names.get("CI_RESULTS_FILE", required=True)))


def errno_name(exc: OSError) -> str:
    """Return the symbolic errno of ``exc`` (``ENOENT``), else its class name."""
    return errno.errorcode.get(exc.errno or 0, type(exc).__name__)


def show_path(path: Path, base: Optional[Path] = None) -> str:
    """Return ``path`` relative to ``base`` (default: the working directory) if it passes the allowlist.

    Anything else prints as ``<path>`` (CR-4): build-tree names are not data,
    but they are still never relayed unchecked.
    """
    base = base if base is not None else Path.cwd()
    try:
        text = Path(os.path.abspath(path)).relative_to(os.path.abspath(base)).as_posix()
    except ValueError:
        text = path.as_posix()
    return text if _PATH_TOKEN.match(text) and ".." not in text.split("/") else "<path>"


# ---------------------------------------------------------------------------
# C17: the shipped binary
# ---------------------------------------------------------------------------

def find_binaries(build_dir: Path) -> List[Path]:
    """Return every shipped ``smbios_gui`` under ``build_dir``, in sorted walk order.

    Same selection as the old ``find <dir> -type f -path '*/smbios_gui/bin/*'
    \\( -name smbios_gui -o -name smbios_gui.exe \\)``: a regular file (a
    symlink is neither followed nor returned), exact (case-sensitive) name,
    and the path, spelled from ``build_dir`` as given, matches
    :data:`BINARY_PATH_PATTERN` (``*`` crosses ``/``). A missing
    ``build_dir`` yields ``[]`` (the caller fails closed on finding nothing).

    Raises:
        OSError: A directory of an existing tree cannot be listed, or a
            candidate cannot be ``lstat``-ed (review S3-4: an unlistable
            subtree could hide a second ``smbios_gui``; never ignored).
    """
    found: List[Path] = []
    if not os.path.lexists(build_dir):
        return found

    def raise_error(exc: OSError) -> None:
        raise exc

    for root, dirs, files in os.walk(build_dir, onerror=raise_error, followlinks=False):
        dirs.sort()
        for name in sorted(files):
            if name not in BINARY_NAMES:
                continue
            candidate = Path(root) / name
            if not fnmatch.fnmatchcase(candidate.as_posix(), BINARY_PATH_PATTERN):
                continue
            if stat.S_ISREG(os.lstat(candidate).st_mode):
                found.append(candidate)
    return found


def check_find_binary(build_dir: Path, ctx: Ctx) -> Tuple[int, Optional[Path]]:
    """C17: print ``ok: smbios_gui: <path> (<bytes> bytes)`` or fail; return ``(exit, binary)``."""
    start = time.monotonic()
    try:
        found = find_binaries(build_dir)
    except OSError as exc:
        return _fail(ctx, LABEL_BINARY, start,
                     f"smbios_gui binary search failed ({errno_name(exc)})",
                     [f"searched: {show_path(build_dir)}"]), None
    if not found:
        return _fail(ctx, LABEL_BINARY, start, "smbios_gui binary: expected 1 -> actual 0",
                     [f"searched: {show_path(build_dir)}"]), None
    binary = found[0]
    try:
        size = os.lstat(binary).st_size
    except OSError as exc:
        return _fail(ctx, LABEL_BINARY, start,
                     f"smbios_gui binary could not be read ({errno_name(exc)})", []), None
    summary = f"{show_path(binary)} ({size} bytes)"
    if len(found) > 1:
        summary += f"; {len(found)} found, all checked for hook strings"
    _ok(ctx, LABEL_BINARY, summary, start)
    return EXIT_OK, binary


# ---------------------------------------------------------------------------
# C18 / R-7: hook strings
# ---------------------------------------------------------------------------

def _open_regular(path: Path) -> int:
    """Open ``path`` read-only if it is a regular file, never through a symlink (CR-12).

    Raises:
        OSError: ``path`` is missing, a symlink (``ELOOP``), a directory
            (``EISDIR``), any other non-regular file (``EINVAL``; checked before
            ``open`` so a FIFO cannot block), changed between the check and
            the open, or unreadable.
    """
    st = os.lstat(path)
    if stat.S_ISLNK(st.st_mode):
        raise OSError(errno.ELOOP, "symlink", str(path))
    if stat.S_ISDIR(st.st_mode):
        raise OSError(errno.EISDIR, "directory", str(path))
    if not stat.S_ISREG(st.st_mode):
        raise OSError(errno.EINVAL, "not a regular file", str(path))
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_BINARY", 0)
    fd = os.open(path, flags)
    try:
        now = os.fstat(fd)
        if not stat.S_ISREG(now.st_mode) or (now.st_dev, now.st_ino) != (st.st_dev, st.st_ino):
            raise OSError(errno.ESTALE, "file changed while opening", str(path))
    except BaseException:
        os.close(fd)
        raise
    return fd


def count_occurrences(path: Path, pattern: bytes, chunk: int = CHUNK) -> int:
    """Return the number of non-overlapping occurrences of ``pattern`` in the file ``path``.

    The file is read in ``chunk``-byte pieces; the last ``len(pattern)-1``
    bytes of each buffer (but nothing inside an already counted match) are
    carried into the next, so a match across a chunk boundary is counted
    exactly once. The result equals ``data.count(pattern)`` over the whole
    file.

    Raises:
        OSError: See :func:`_open_regular`, or a read error.
        ValueError: Empty ``pattern`` or ``chunk < 1``.
    """
    if not pattern or chunk < 1:
        raise ValueError("pattern must be non-empty and chunk positive")
    width = len(pattern)
    count = 0
    carry = b""
    fd = _open_regular(path)
    with os.fdopen(fd, "rb", buffering=0) as fh:
        while True:
            data = fh.read(chunk)
            if not data:
                break
            buf = carry + data
            pos = 0
            last_end = 0
            while True:
                hit = buf.find(pattern, pos)
                if hit < 0:
                    break
                count += 1
                pos = last_end = hit + width
            carry = buf[max(len(buf) - (width - 1), last_end):]
    return count


def regular_file_size(path: Path) -> int:
    """Return the size of the regular file ``path``, read from the opened descriptor.

    Raises:
        OSError: See :func:`_open_regular`.
    """
    fd = _open_regular(path)
    try:
        return os.fstat(fd).st_size
    finally:
        os.close(fd)


def check_hook_strings(binaries: Sequence[Path], ctx: Ctx,
                       min_size: int = MIN_BINARY_BYTES) -> int:
    """R-7: every pattern of :data:`HOOK_PATTERNS` occurs 0 times in every binary; return the exit.

    No binary at all, any binary that cannot be read, or any binary smaller
    than ``min_size`` bytes (empty or truncated, review S3-2) fails closed. A
    hook string found is reported first, since it fails whatever the size.
    """
    start = time.monotonic()
    if not binaries:
        return _fail(ctx, LABEL_HOOKS, start,
                     "hook string check: smbios_gui binary: expected 1 -> actual 0", [])
    totals = {p: 0 for p in HOOK_PATTERNS}
    small: List[Tuple[Path, int]] = []
    for binary in binaries:
        try:
            size = regular_file_size(binary)
            for pattern in HOOK_PATTERNS:
                totals[pattern] += count_occurrences(binary, pattern)
        except OSError as exc:
            return _fail(ctx, LABEL_HOOKS, start,
                         f"hook string check could not read the shipped smbios_gui "
                         f"({errno_name(exc)})", [f"binary: {show_path(binary)}"])
        if size < min_size:
            small.append((binary, size))
    bad = [(p, n) for p, n in totals.items() if n != 0]
    if bad:
        detail = [f"'{p.decode('ascii')}': expected 0 -> {n}" for p, n in totals.items()]
        return _fail(ctx, LABEL_HOOKS, start,
                     f"test hook present in shipped smbios_gui: expected 0 -> {bad[0][1]}", detail)
    if small:
        binary, size = small[0]
        return _fail(ctx, LABEL_HOOKS, start,
                     f"smbios_gui binary size: expected >= {min_size} -> {size}",
                     [f"binary: {show_path(binary)}"])
    _ok(ctx, OK_HOOKS, "", start)
    return EXIT_OK


def check_hook_strings_in_tree(build_dir: Path, ctx: Ctx) -> int:
    """R-7 over every binary :func:`find_binaries` returns; a search error fails closed (S3-4)."""
    start = time.monotonic()
    try:
        binaries = find_binaries(build_dir)
    except OSError as exc:
        return _fail(ctx, LABEL_HOOKS, start,
                     f"hook string check: smbios_gui binary search failed ({errno_name(exc)})",
                     [f"searched: {show_path(build_dir)}"])
    return check_hook_strings(binaries, ctx)


# ---------------------------------------------------------------------------
# C20 / R-8: the local-only tier is absent
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class CtestListing:
    """The parsed outcome of one ``ctest -N`` run.

    Attributes:
        rc: ctest's exit code; ``None`` if it could not start, 124 on a time-out.
        totals: Every ``Total Tests: <n>`` value, in output order.
        error: Why ctest did not run (an errno name), else empty.
    """

    rc: Optional[int]
    totals: Tuple[int, ...] = field(default_factory=tuple)
    error: str = ""


def parse_totals(output: bytes) -> Tuple[int, ...]:
    """Return every ``Total Tests: <n>`` value of ``output`` (CR stripped, per line)."""
    found: List[int] = []
    for line in output.replace(b"\r", b"").split(b"\n"):
        match = _TOTAL_TESTS.match(line)
        if match is not None:
            found.append(int(match.group(1)))
    return tuple(found)


def ctest_listing(argv: Sequence[str], timeout_s: float = CTEST_TIMEOUT_S) -> CtestListing:
    """Run ``argv`` (a ``ctest -N`` command, no shell) and parse its merged output."""
    try:
        proc = subprocess.run(list(argv), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              stdin=subprocess.DEVNULL, timeout=timeout_s, check=False,
                              env=child_environment(os.environ))
    except subprocess.TimeoutExpired:
        return CtestListing(ci_step.EXIT_TIMEOUT, (), "timeout")
    except OSError as exc:
        return CtestListing(None, (), errno_name(exc))
    return CtestListing(proc.returncode, parse_totals(proc.stdout or b""))


def ctest_total(argv: Sequence[str]) -> Optional[int]:
    """Return ``n`` of the one ``Total Tests: <n>`` line, or ``None`` (the caller fails closed).

    ``None`` when ctest could not run, exited non-zero, or printed zero or
    more than one such line (CR-12).
    """
    listing = ctest_listing(argv)
    if listing.rc != 0 or len(listing.totals) != 1:
        return None
    return listing.totals[0]


def ctest_argv(build_dir: Path, config: Optional[str], select: Sequence[str]) -> List[str]:
    """Return ``ctest --test-dir <dir> [-C <config>] -N <select>``."""
    argv = ["ctest", "--test-dir", str(build_dir)]
    if config:
        argv += ["-C", config]
    return argv + ["-N", *select]


def _listing_problem(what: str, listing: CtestListing) -> Optional[str]:
    """Return the discrepancy for one listing, or ``None`` if it proves 0 tests."""
    if listing.rc is None:
        return f"{what}: ctest could not run ({listing.error})"
    if not listing.totals:
        return f"{what}: 'Total Tests:' line: expected present -> missing (exit {listing.rc})"
    if len(listing.totals) != 1:
        return f"{what}: 'Total Tests:' lines: expected 1 -> {len(listing.totals)}"
    if listing.rc != 0:
        return f"{what}: ctest exit: expected 0 -> {listing.rc}"
    if listing.totals[0] != 0:
        return f"{what}: local-only GUI tests are configured on CI: expected 0 -> {listing.totals[0]}"
    return None


def _configured_tree_problems(build_dir: Path, config: Optional[str]) -> List[str]:
    """Return why ``build_dir`` is not a configured GUI CTest build tree (review S3-3), or ``[]``.

    ``ctest -N`` prints ``Total Tests: 0`` and exits 0 in any directory, so the
    two zero counts of R-8 only mean something inside a configured tree: it
    must hold a regular ``CTestTestfile.cmake`` and list at least one
    ``gui-ci`` test.
    """
    problems: List[str] = []
    try:
        mode = os.lstat(Path(build_dir) / CTEST_FILE).st_mode
    except OSError as exc:
        problems.append(f"configured build tree: {CTEST_FILE}: expected regular file -> "
                        f"{errno_name(exc)}")
    else:
        if not stat.S_ISREG(mode):
            problems.append(f"configured build tree: {CTEST_FILE}: expected regular file -> "
                            "not a regular file")
    what = "ctest -N -L gui-ci"
    listing = ctest_listing(ctest_argv(build_dir, config, CONFIGURED_ARGS))
    if listing.rc is None:
        problems.append(f"{what}: ctest could not run ({listing.error})")
    elif len(listing.totals) != 1:
        problems.append(f"{what}: 'Total Tests:' lines: expected 1 -> {len(listing.totals)} "
                        f"(exit {listing.rc})")
    elif listing.rc != 0:
        problems.append(f"{what}: ctest exit: expected 0 -> {listing.rc}")
    elif listing.totals[0] < 1:
        problems.append(f"{what}: configured gui-ci tests: expected > 0 -> {listing.totals[0]} "
                        "(not a configured GUI build tree)")
    return problems


def local_tier_artifacts(build_dir: Path) -> List[Path]:
    """Return every entry under ``build_dir`` named like a local-only GUI artifact, sorted.

    Files, directories and symlinks (never followed) are all checked, by
    ``casefold()``-ed name prefix (``find`` without ``-type f``, CR-12).

    Raises:
        OSError: ``build_dir`` is missing or not a directory, or any directory
            of the tree cannot be listed (``find`` exiting non-zero).
    """
    if not os.path.isdir(build_dir):
        raise OSError(errno.ENOTDIR if os.path.exists(build_dir) else errno.ENOENT,
                      "build tree missing", str(build_dir))
    prefixes = tuple(p.casefold() for p in LOCAL_PREFIXES)
    found: List[Path] = []

    def raise_error(exc: OSError) -> None:
        raise exc

    root_name = Path(os.path.abspath(build_dir)).name
    if root_name.casefold().startswith(prefixes):
        found.append(Path(build_dir))
    for root, dirs, files in os.walk(build_dir, onerror=raise_error, followlinks=False):
        for name in list(dirs) + list(files):
            if name.casefold().startswith(prefixes):
                found.append(Path(root) / name)
    return sorted(found)


def check_local_tier_absent(build_dir: Path, config: Optional[str], ctx: Ctx) -> int:
    """R-8: 0 gui-local tests in a configured tree and no local artifact; return the exit."""
    start = time.monotonic()
    if config and not _CONFIG.match(config):
        return _fail(ctx, LABEL_LOCAL, start, "config must match [A-Za-z0-9_]+", [])
    problems: List[str] = []
    for what, select in (("ctest -N -L gui-local", LOCAL_LABEL_ARGS),
                         ("ctest -N -R smbios_gui_local_test|smbios_gui_autoexit",
                          LOCAL_NAME_ARGS)):
        problem = _listing_problem(what, ctest_listing(ctest_argv(build_dir, config, select)))
        if problem is not None:
            problems.append(problem)
    try:
        artifacts = local_tier_artifacts(build_dir)
    except OSError as exc:
        problems.append(f"local-only GUI file check: walk failed ({errno_name(exc)})")
    else:
        if artifacts:
            problems.append(f"local-only GUI files exist on CI: expected 0 -> {len(artifacts)}")
            problems += [f"  {show_path(p, build_dir)}" for p in artifacts[:MAX_SHOWN_PATHS]]
    problems += _configured_tree_problems(build_dir, config)
    if problems:
        return _fail(ctx, LABEL_LOCAL, start, problems[0], problems[1:])
    _ok(ctx, OK_LOCAL, "", start)
    return EXIT_OK


# ---------------------------------------------------------------------------
# RC1 / RC3 output
# ---------------------------------------------------------------------------

def _ok(ctx: Ctx, label: str, summary: str, start: float) -> None:
    elapsed = round(time.monotonic() - start, 1)
    ci_step.emit(ctx.reporter.step_ok(label, summary, elapsed))
    ci_step.append_record(ci_step.StepResult(label, "hard", "ok", 0, elapsed, summary, None, None),
                          ctx.results)


def _fail(ctx: Ctx, label: str, start: float, first: str, rest: Sequence[str]) -> int:
    elapsed = round(time.monotonic() - start, 1)
    ci_step.emit(ctx.reporter.step_fail(label, EXIT_GATE, elapsed, first,
                                        list(rest)[:ci_step.MAX_DIAG]))
    ci_step.append_record(ci_step.StepResult(label, "hard", "fail", EXIT_GATE, elapsed, "", first,
                                             None), ctx.results)
    return EXIT_GATE


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------

def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="gui_guards.py",
                                     description="GUI binary search and the R-7/R-8 guards.")
    parser.add_argument("--reporter", default=None, help="github|teamcity|plain")
    sub = parser.add_subparsers(dest="command", required=True)
    find = sub.add_parser("find-binary", help="C17: locate the shipped smbios_gui")
    find.add_argument("--build-dir", required=True, type=Path)
    hooks = sub.add_parser("hook-strings", help="R-7: no test-hook strings in the binary")
    hooks.add_argument("--binary", required=True, type=Path)
    local = sub.add_parser("local-tier-absent", help="R-8: no gui-local tier configured")
    local.add_argument("--build-dir", required=True, type=Path)
    local.add_argument("--config", default=None)
    return parser


def main(argv: Sequence[str]) -> int:
    """Run one subcommand; return 0, 1 (a check failed) or 2 (usage/environment)."""
    args = _parser().parse_args(list(argv))
    try:
        ctx = default_ctx(get_reporter(args.reporter))
    except ValueError:
        ci_step.emit_error("gui_guards.py: error: reporter must be one of: github, teamcity, plain")
        return EXIT_USAGE
    if args.command == "find-binary":
        return check_find_binary(args.build_dir, ctx)[0]
    if args.command == "hook-strings":
        return check_hook_strings([args.binary], ctx)
    return check_local_tier_absent(args.build_dir, args.config, ctx)


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except KeyboardInterrupt:
        sys.exit(130)
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2); never a pass
        ci_step.emit([f"error: gui_guards.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
