#!/usr/bin/env python3
"""Step runner implementing the CI report contract (milestone 0014, RC1-RC3, RC5).

Every project-owned CI step runs through this script, which generalises
milestone 0011's ``run_quiet``:

- The command runs **without a shell** (argv list, ``shell=False``, stdin
  closed). Its stdout+stderr stream to ``<logdir>/<label>.log``; nothing of a
  passing command is printed.
- **Success** prints exactly one line: ``ok: <label>[: <summary>] (<elapsed>s)``.
- **Failure** prints one annotation, then at most 20 diagnostic lines (the
  selected pattern sets, each prefixed ``L<line>: ``), then at most 40 tail
  lines that leave out lines already shown - never more than 64 lines and
  16 KiB in total, each excerpt line cut to 200 characters and stripped of
  ESC/control characters. The full log stays on disk.
- **Best-effort** (``--best-effort "<reason>"``, RC5) prints a warning plus the
  same excerpt on a non-zero exit, and always exits 0.
- Every excerpt of tool output is fenced so the CI platform cannot interpret a
  hostile line as a command (threat model T-3). The fence, the annotation
  syntax and the escaping belong to the reporter backend (``reporters/``,
  ADR 0005), selected by ``--reporter`` or ``$CI_REPORTER`` (default
  ``plain``). Truncation, ceilings and hygiene happen here, before rendering,
  so they are identical for every backend.
- Each ``run`` appends one RC3 JSON record to ``$CI_RESULTS_FILE``.
- **Data hygiene** (``--hygiene counts-only``, RC1, threat model CR-9): a step
  that reads the live SMBIOS table shows only CTest status lines and the fixed
  CTest summary lines, and logs under ``$CI_LOG_DIR/live/``.
- **Environment scrub** (CR-5): a child never inherits the platform's
  file-command channels, OIDC/runtime tokens, publish or cloud credentials,
  or debug switches, unless the caller passes a name via ``pass_env``.
- **File safety** (CR-11): files the runner creates are private (umask 077,
  mode 0600); a log is never written through a symlink; a group- or
  world-writable log directory the runner did not create is refused (exit 2).

Exit codes: hard mode returns the child's code unchanged (a signal maps to
128+n), 127 when the child cannot be started, 124 when a caller-set
``run_step(timeout_s=...)`` expired, 0 for best-effort, 2 for a usage
or environment error, 70 for a bug in this runner (reported as one line, never
a traceback).

Usage::

    python3 scripts/ci/ci_step.py run --label <label>
            [--patterns <set>[,<set>...]] [--best-effort "<reason>"]
            [--summary "<text>"] [--max-diag 20] [--tail 40]
            [--hygiene full|counts-only]
            [--reporter github|teamcity|plain] -- <command> [args...]
    python3 scripts/ci/ci_step.py self-test

Contract: ``docs/roadmap/0014-ci-rework-from-scratch/plan.md`` ("Shared
contracts") and ``docs/adr/0005-unified-build-package-publish-pipeline.md``
("Platform portability"). Python 3 standard library only.
"""

from __future__ import annotations

import os
import sys

# Works under ``python3 -I`` (which drops the script directory from sys.path)
# and from any working directory: sibling modules are found next to this file.
_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import argparse  # noqa: E402
import contextlib  # noqa: E402
import io  # noqa: E402
import json  # noqa: E402
import platform  # noqa: E402
import re  # noqa: E402
import stat  # noqa: E402
import subprocess  # noqa: E402
import tempfile  # noqa: E402
import time  # noqa: E402
from collections import deque  # noqa: E402
from dataclasses import asdict, dataclass  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import (Callable, Dict, Iterator, List, Literal, Optional, Pattern,  # noqa: E402
                    Sequence, Tuple)

import ci_patterns  # noqa: E402
import env_names  # noqa: E402
from reporters import Reporter, child_environment, get_reporter, BACKEND_NAMES  # noqa: E402,F401
from reporters.github import escape_annotation  # noqa: E402,F401  (re-exported, RC1 helper)

# --- RC1 thresholds (a change is a plan.md amendment, not a code tweak) -----
MAX_DIAG = 20
MAX_TAIL = 40
LINE_WIDTH = 200
SUMMARY_MAX = 160
STEP_MAX_LINES = 64
STEP_MAX_BYTES = 16 * 1024
#: Lines/bytes kept free for the backend's annotation and fence lines, so the
#: rendered step still fits the ceiling (1 annotation + 2 fence lines).
_RESERVED_LINES = 3
_RESERVED_BYTES = 1024
_EXCERPT_MAX_LINES = STEP_MAX_LINES - _RESERVED_LINES
_EXCERPT_MAX_BYTES = STEP_MAX_BYTES - _RESERVED_BYTES

#: Longest raw line prefix read from the log; the rest of an overlong line is
#: skipped without being held in memory.
_READ_CHUNK = 4096

EXIT_USAGE = 2
EXIT_INTERNAL = 70
EXIT_NOT_STARTED = 127
#: A step killed by its ``timeout_s`` (coreutils ``timeout`` convention).
EXIT_TIMEOUT = 124

# Whole ANSI CSI/OSC/two-byte escape sequences, removed before single controls
# so no "[31m" residue is left behind.
_ANSI = re.compile(r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07\x1b]*(?:\x07|\x1b\\)?|[@-Z\\-_])")
# C0 controls except TAB, DEL, C1 controls, and bidi/line-separator code points.
_UNSAFE = re.compile("[\x00-\x08\x0a-\x1f\x7f-\x9f  ‪-‮⁦-⁩]")
_LABEL_OK = re.compile(r"^[A-Za-z0-9][A-Za-z0-9 ._/+()-]{0,79}$")

#: RC1 data hygiene modes: ``full`` (the default excerpt) or ``counts-only``
#: (CTest status and summary lines only, for steps that read the live table).
Hygiene = Literal["full", "counts-only"]
HYGIENE_MODES: Tuple[str, ...] = ("full", "counts-only")

#: The anchored CTest status-line regex of threat model CR-9. Under
#: ``counts-only`` only the matched part (plus an optional ``<n> sec``
#: timing) is kept, so text after the status word never leaves the log.
CTEST_STATUS_LINE = re.compile(
    r"^\s*\d+/\d+ Test +#\d+: \S+ \.+\s*(\*{3}\w+|Passed|Failed|Not Run)(?: +[0-9.]+ sec)?")

#: The fixed CTest summary lines (``tests passed``, ``Total Test time``),
#: matched in full against the stripped line.
CTEST_SUMMARY_LINES: Tuple[Pattern[str], ...] = (
    re.compile(r"^\d+% tests passed, \d+ tests? failed out of \d+$"),
    re.compile(r"^Total Test time \(real\) = +[0-9.]+ sec$"),
)

#: ``first_diagnostic`` of a ``counts-only`` step with no failing status line.
SEE_STATUS_LINES = "see status lines"

#: Sub-directory of the log directory for ``counts-only`` steps; no upload
#: path names it (CR-9, CR-10).
LIVE_LOG_SUBDIR = "live"


def counts_only_line(text: str) -> Optional[str]:
    """Return the part of ``text`` that ``counts-only`` may show, or ``None``.

    Only a CTest status line (its matched prefix) or a fixed CTest summary
    line passes; everything else from the log is dropped (CR-9).
    """
    m = CTEST_STATUS_LINE.match(text)
    if m:
        return m.group(0)
    stripped = text.strip()
    if any(rx.match(stripped) for rx in CTEST_SUMMARY_LINES):
        return stripped
    return None


class UnsafePathError(OSError):
    """A log path or log directory the runner refuses to write (threat model CR-11)."""


@dataclass(frozen=True)
class StepResult:
    """One RC3 record: the outcome of one ``run`` call."""

    label: str
    mode: Literal["hard", "best-effort", "info"]
    outcome: Literal["ok", "fail", "warn", "skip"]
    exit: int
    elapsed_s: float
    summary: str
    first_diagnostic: Optional[str]
    counts: Optional[Dict[str, int]]


@dataclass(frozen=True)
class StepFinish:
    """What a caller's ``finish`` hook adds to a step after its child exited (Story 03.0 Task 02).

    Used by ``ctest_report.py`` so that counts read from structured output
    (``ctest --output-junit``) land in the same single RC1 line and RC3
    record as the step itself, instead of a second report.

    Attributes:
        summary: Replaces the ``ok:`` summary when not ``None`` (cut to 160).
        counts: The RC3 ``counts`` object (``passed``/``failed``/``skipped``/
            ``not_run``), or ``None`` when no count could be read.
        first: Replaces ``first_diagnostic`` of a failing step when not
            ``None`` (ignored for a timeout or a child that never started).
        lines: Printed first in a failing step's excerpt (for example RC1
            discrepancy lines); each goes through :func:`safe_line`. The
            caller keeps them within the RC1 bounds (at most 22 lines).
    """

    summary: Optional[str] = None
    counts: Optional[Dict[str, int]] = None
    first: Optional[str] = None
    lines: Tuple[str, ...] = ()


@dataclass
class _Scan:
    """Result of one bounded pass over a log file."""

    diag: List[Tuple[int, str]]
    diag_total: int
    tail: List[Tuple[int, str]]


def safe_line(raw: bytes, width: int = LINE_WIDTH) -> str:
    """Decode one raw log line for display: invalid UTF-8 replaced, controls stripped, cut.

    ESC (and so every ANSI sequence's introducer), all other C0/C1 controls
    except TAB, CR/LF, DEL and bidi-override code points are removed; the
    result is cut to ``width`` characters.
    """
    body = raw.rstrip(b"\r\n")
    if body.isascii():  # fast path for the common clean line
        text = body.decode("ascii")
        if text.isprintable():
            return text[:width]
    text = raw.decode("utf-8", errors="replace")
    return _UNSAFE.sub("", _ANSI.sub("", text))[:width]


def _iter_lines(path: Path) -> Iterator[Tuple[int, bytes]]:
    """Yield ``(line_number, raw_prefix)`` with bounded memory per line."""
    with open(path, "rb") as fh:
        number = 0
        while True:
            chunk = fh.readline(_READ_CHUNK)
            if not chunk:
                return
            number += 1
            prefix = chunk
            while not chunk.endswith(b"\n") and len(chunk) == _READ_CHUNK:
                chunk = fh.readline(_READ_CHUNK)  # discard the rest of a long line
                if not chunk:
                    break
            yield number, prefix


def _scan_log(log_path: Path, regexes: Sequence[Pattern[str]], max_diag: int,
              tail: int, keep: Optional[Callable[[str], Optional[str]]] = None) -> _Scan:
    """Stream the log once: first ``max_diag`` matches, total matches, last lines.

    ``keep``, when given, maps each sanitised line to the text that may be
    shown, or ``None`` to drop the line entirely (``counts-only`` hygiene).
    """
    diag: List[Tuple[int, str]] = []
    total = 0
    window: deque = deque(maxlen=tail + max_diag)
    try:
        lines = _iter_lines(log_path)
        for number, raw in lines:
            text = safe_line(raw, _READ_CHUNK)
            if keep is not None:
                kept_text = keep(text)
                if kept_text is None:
                    continue
                text = kept_text
            if any(rx.search(text) for rx in regexes):
                total += 1
                if len(diag) < max_diag:
                    diag.append((number, text))
            window.append((number, text))
    except FileNotFoundError:
        pass
    shown = {n for n, _ in diag}
    kept = [(n, t) for n, t in window if n not in shown]
    return _Scan(diag=diag, diag_total=total, tail=kept[-tail:] if tail else [])


def _excerpt(label: str, scan: _Scan) -> List[str]:
    """Build the neutral RC1 excerpt (blocks 2 and 3) inside the excerpt budget."""
    diag = [f"L{n}: {t}"[:LINE_WIDTH] for n, t in scan.diag]
    tail = [t[:LINE_WIDTH] for _, t in scan.tail]

    def build() -> List[str]:
        out: List[str] = []
        if scan.diag_total:
            out.append(f"--- {label}: first {len(diag)} of {scan.diag_total} diagnostic lines ---")
            out.extend(diag)
        if tail:
            out.append(f"--- {label}: last {len(tail)} lines ---")
            out.extend(tail)
        return out

    lines = build()
    # Trim oldest tail lines first, then the last diagnostic lines, until both
    # the line and the byte budget hold.
    while lines and (len(lines) > _EXCERPT_MAX_LINES
                     or len("\n".join(lines).encode("utf-8")) > _EXCERPT_MAX_BYTES):
        if tail:
            tail.pop(0)
        elif diag:
            diag.pop()
        else:
            break
        lines = build()
    return lines


def render_failure(label: str, log_path: Path, patterns: Sequence[str],
                   max_diag: int, tail: int) -> List[str]:
    """Return the neutral RC1 failure excerpt for ``log_path``.

    The excerpt is the diagnostic block (at most ``max_diag`` lines matching the
    named pattern sets, each ``L<line>: ``-prefixed) followed by the tail block
    (at most ``tail`` lines, leaving out lines already in the diagnostic block).
    It is sized so that, with the backend's annotation and fence lines, the
    step never exceeds 64 lines / 16 KiB. The log is streamed, never loaded.
    """
    scan = _scan_log(log_path, ci_patterns.compile_sets(patterns),
                     min(max_diag, MAX_DIAG), min(tail, MAX_TAIL))
    return _excerpt(label, scan)


@contextlib.contextmanager
def _private_umask() -> Iterator[None]:
    """Apply ``umask 077`` while the runner creates files and directories (CR-11).

    Scoped, not process-wide, so a child (a build or install step) keeps the
    caller's umask for the files it creates.
    """
    previous = os.umask(0o077)
    try:
        yield
    finally:
        os.umask(previous)


def prepare_log_dir(path: Path) -> Path:
    """Create ``path`` private (0700), or check that an existing one is safe (CR-11).

    Raises:
        UnsafePathError: ``path`` exists, was not created here, and is not a
            directory or is group- or world-writable (POSIX only; Windows
            has no such mode bits).
        OSError: The directory cannot be created.
    """
    with _private_umask():
        try:
            path.mkdir(parents=True, mode=0o700)
            return path  # created by this runner
        except FileExistsError:
            pass
    st = os.stat(str(path))
    if not stat.S_ISDIR(st.st_mode):
        raise UnsafePathError(f"log directory {path.name!r} is not a directory")
    if os.name == "posix" and st.st_mode & (stat.S_IWGRP | stat.S_IWOTH):
        raise UnsafePathError(f"log directory {path.name!r} is group- or world-writable")
    return path


def _open_log(path: Path):
    """Open ``path`` for writing, mode 0600, refusing a symlink (CR-11).

    POSIX uses ``O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW``, so a planted symlink
    fails before anything is truncated; the explicit check covers Windows.

    Raises:
        UnsafePathError: ``path`` is a symlink; its target is left unchanged.
    """
    if path.is_symlink():
        raise UnsafePathError(f"refusing symlinked log {path.name!r}")
    flags = (os.O_WRONLY | os.O_CREAT | os.O_TRUNC | getattr(os, "O_NOFOLLOW", 0)
             | getattr(os, "O_BINARY", 0))
    with _private_umask():
        try:
            fd = os.open(str(path), flags, 0o600)
        except OSError as exc:
            if path.is_symlink():  # ELOOP (Linux/macOS) or EMLINK (FreeBSD)
                raise UnsafePathError(f"refusing symlinked log {path.name!r}") from exc
            raise
    if hasattr(os, "fchmod"):
        os.fchmod(fd, 0o600)  # an existing log may have had a wider mode
    return os.fdopen(fd, "wb")


def append_record(result: StepResult, path: Path) -> None:
    """Append ``result`` as one RC3 JSON line to ``path`` with a single append write.

    A new results file is created with mode 0600 and never through a symlink
    (CR-11).
    """
    data = (json.dumps(asdict(result), ensure_ascii=True, sort_keys=False) + "\n").encode("ascii")
    with _private_umask():
        path.parent.mkdir(parents=True, exist_ok=True)
        fd = os.open(str(path), os.O_WRONLY | os.O_CREAT | os.O_APPEND
                     | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_BINARY", 0), 0o600)
    try:
        written = os.write(fd, data)
        if written != len(data):
            raise OSError(f"short write to results file ({written}/{len(data)} bytes)")
    finally:
        os.close(fd)


def _within_ceiling(lines: List[str]) -> bool:
    return (len(lines) <= STEP_MAX_LINES
            and len("\n".join(lines).encode("utf-8")) <= STEP_MAX_BYTES)


def emit(lines: List[str], *, stderr: bool = False) -> None:
    """Print already-rendered reporter lines in one write (the single emitter, P11).

    Every ``scripts/ci`` script prints only through this function, and only
    lines a reporter backend rendered (``step_*``, ``info``, ``internal_error``)
    or fixed runner text. ``stderr=True`` is for usage/environment errors.
    """
    if lines:
        stream = sys.stderr if stderr else sys.stdout
        stream.write("\n".join(lines) + "\n")
        stream.flush()


#: Story 01.0 name of :func:`emit`, kept for existing callers.
_emit = emit


def emit_error(text: str) -> None:
    """Print one usage/environment error line to stderr through the plain reporter."""
    emit(get_reporter("plain").info(text), stderr=True)


def _render_bounded(render, excerpt: List[str]) -> List[str]:
    """Render with the backend, shrinking the excerpt until the RC1 ceiling holds."""
    excerpt = list(excerpt)
    lines = render(excerpt)
    while excerpt and not _within_ceiling(lines):
        excerpt.pop()
        lines = render(excerpt)
    return lines


def _log_name(label: str) -> str:
    return re.sub(r"[^A-Za-z0-9._-]", "_", label) + ".log"


def log_path_for(label: str, log_dir: Path) -> Path:
    """Return the full-log path :func:`run_step` uses for ``label`` under ``log_dir``."""
    return log_dir / _log_name(label)


#: Grace period between SIGTERM and SIGKILL when a step's ``timeout_s`` expires.
_KILL_GRACE_S = 5.0


def _run_with_timeout(argv: Sequence[str], log, timeout_s: float,
                      env: Dict[str, str]) -> Tuple[int, bool]:
    """Run ``argv`` with a time limit; return ``(returncode, timed_out)``.

    On POSIX the child starts in its own session so the whole process group is
    signalled: SIGTERM first (``sudo`` relays it to the command it runs, which
    an unprivileged caller could not signal directly), SIGKILL after
    :data:`_KILL_GRACE_S`. Elsewhere the child itself is killed.
    """
    posix = os.name == "posix"
    proc = subprocess.Popen(list(argv), shell=False, stdout=log, stderr=subprocess.STDOUT,
                            stdin=subprocess.DEVNULL, env=env, start_new_session=posix)
    try:
        return proc.wait(timeout=timeout_s), False
    except subprocess.TimeoutExpired:
        pass
    if posix:
        import signal
        for sig, grace in ((signal.SIGTERM, _KILL_GRACE_S), (signal.SIGKILL, _KILL_GRACE_S)):
            try:
                os.killpg(proc.pid, sig)
            except (ProcessLookupError, PermissionError):
                with contextlib.suppress(ProcessLookupError, PermissionError):
                    proc.send_signal(sig)
            try:
                proc.wait(timeout=grace)
                break
            except subprocess.TimeoutExpired:
                continue
    else:
        proc.kill()
        with contextlib.suppress(subprocess.TimeoutExpired):
            proc.wait(timeout=_KILL_GRACE_S)
    return (proc.returncode if proc.returncode is not None else EXIT_TIMEOUT), True


def _local_log_note(reporter: Reporter, log_dir: Path) -> List[str]:
    """Return the one ``info: logs kept in <dir>`` line of a local ``plain`` run (CR-11).

    Printed only with a failing or warning step whose child ran, so RC1's
    one-line success (and ``lint.py``'s three-line success) is unchanged.
    """
    if reporter.name != "plain" or env_names.get("CI_PLATFORM") != "local":
        return []
    return reporter.info(f"info: logs kept in {log_dir}")


def run_step(label: str, argv: Sequence[str], *, patterns: Sequence[str],
             best_effort_reason: Optional[str], summary: Optional[str],
             max_diag: int = MAX_DIAG, tail: int = MAX_TAIL, log_dir: Path,
             reporter: Optional[Reporter] = None,
             results_file: Optional[Path] = None,
             timeout_s: Optional[float] = None,
             hygiene: Hygiene = "full",
             pass_env: Sequence[str] = (),
             finish: Optional[Callable[[int], StepFinish]] = None) -> StepResult:
    """Run one step under the report contract, print its RC1 output, append its RC3 record.

    Args:
        label: Step label (log file name, annotation title, record key).
        argv: Command and arguments; executed without a shell.
        patterns: Diagnostic pattern-set names (``ci_patterns.PATTERN_SETS``).
        best_effort_reason: Non-``None`` makes the step best-effort (RC5).
        summary: Text appended to the ``ok:`` line (cut to 160 characters).
        max_diag: Diagnostic lines to show (at most 20).
        tail: Tail lines to show (at most 40).
        log_dir: Directory for ``<label>.log``.
        reporter: Backend; ``None`` selects via :func:`get_reporter`.
        results_file: RC3 file; ``None`` means ``$CI_RESULTS_FILE``.
        timeout_s: Optional runner-side time limit (added by Story 02.0 Task 02
            for network-dependent steps). On expiry the child (and on POSIX its
            process group) gets SIGTERM, then SIGKILL after a grace period; the
            step fails with exit :data:`EXIT_TIMEOUT` and the usual RC1 excerpt.
            ``None`` (the default) keeps the original unbounded behaviour.
        hygiene: ``"counts-only"`` (Story 02.0 Task 04, RC1 data hygiene,
            CR-9) for a step that reads the live SMBIOS table: the excerpt and
            ``first_diagnostic`` come only from CTest status and summary lines
            (else ``first_diagnostic`` is ``"see status lines"``), and the log
            goes to ``<log_dir>/live/``. ``"full"`` (default) is unchanged.
        pass_env: Names on the CR-5 child-environment denylist that this
            child may still inherit, by exact name (for example
            ``("AWS_WEB_IDENTITY_TOKEN_FILE",)``). Default: none.
        finish: Optional hook called once with the mapped exit code after
            the child exited and before anything is printed; its
            :class:`StepFinish` supplies the summary, RC3 counts, first
            diagnostic and leading excerpt lines (Story 03.0 Task 02,
            ``ctest_report.py``). The exit code alone still decides the
            outcome.

    Returns:
        The step's record; ``result.exit`` is the child's mapped exit code.

    Raises:
        UnsafePathError: The log directory is unsafe, or ``<label>.log`` is a
            symlink (CR-11). Nothing was run and no record was written.
        ValueError: ``hygiene`` is not one of :data:`HYGIENE_MODES`.
    """
    if hygiene not in HYGIENE_MODES:
        raise ValueError(f"unknown hygiene mode {hygiene!r}")
    counts_only = hygiene == "counts-only"
    reporter = reporter or get_reporter(None)
    results = results_file or Path(env_names.get("CI_RESULTS_FILE", required=True))
    regexes = ci_patterns.compile_sets(patterns)
    max_diag = max(0, min(max_diag, MAX_DIAG))
    tail = max(0, min(tail, MAX_TAIL))
    summary_text = safe_line((summary or "").encode("utf-8"), SUMMARY_MAX)
    mode = "best-effort" if best_effort_reason is not None else "hard"
    reason = safe_line((best_effort_reason or "").encode("utf-8"), SUMMARY_MAX)

    prepare_log_dir(log_dir)
    if counts_only:
        log_dir = prepare_log_dir(log_dir / LIVE_LOG_SUBDIR)
    log_path = log_dir / _log_name(label)
    child_env = child_environment(os.environ, pass_env)
    start = time.monotonic()
    not_started: Optional[str] = None
    timed_out = False
    with _open_log(log_path) as log:
        try:
            if timeout_s is None:
                proc = subprocess.run(list(argv), shell=False, stdout=log,
                                      stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                      env=child_env)
                rc = proc.returncode
            else:
                rc, timed_out = _run_with_timeout(argv, log, timeout_s, child_env)
        except (OSError, ValueError) as exc:  # not found, not executable, bad argv
            rc = EXIT_NOT_STARTED
            detail = exc.strerror if isinstance(exc, OSError) and exc.strerror else type(exc).__name__
            not_started = safe_line(f"cannot start {argv[0] if argv else '<empty>'}: {detail}"
                                    .encode("utf-8"), SUMMARY_MAX)
    elapsed = round(time.monotonic() - start, 1)
    if timed_out:
        rc = EXIT_TIMEOUT
    elif rc < 0:
        rc = 128 - rc  # killed by signal n -> 128+n

    fin = finish(rc) if finish is not None else StepFinish()
    counts = dict(fin.counts) if fin.counts is not None else None
    if fin.summary is not None:
        summary_text = safe_line(fin.summary.encode("utf-8"), SUMMARY_MAX)
    lead = [safe_line(x.encode("utf-8"), LINE_WIDTH) for x in fin.lines]

    if rc == 0:
        lines = reporter.step_ok(label, summary_text, elapsed)
        result = StepResult(label, mode, "ok", 0, elapsed, summary_text, None, counts)
    else:
        if not_started is not None:
            excerpt: List[str] = []
            first = not_started
        else:
            scan = _scan_log(log_path, regexes, max_diag, max(0, tail - len(lead)),
                             keep=counts_only_line if counts_only else None)
            excerpt = lead + _excerpt(label, scan)
            if timed_out:
                first = f"timed out after {timeout_s:g}s"
            elif fin.first is not None:
                first = safe_line(fin.first.encode("utf-8"), SUMMARY_MAX)
            elif counts_only:
                first = scan.diag[0][1] if scan.diag else SEE_STATUS_LINES
            elif scan.diag:
                first = scan.diag[0][1]
            elif scan.tail:
                first = next((t for _, t in reversed(scan.tail) if t.strip()), "no output")
            else:
                first = "no output"
            first = first.strip()[:SUMMARY_MAX] or "no output"
        # No note when the child never started: its log is empty.
        note = _local_log_note(reporter, log_dir) if not_started is None else []
        if mode == "best-effort":
            lines = _render_bounded(
                lambda ex: reporter.step_warn(label, reason, rc, ex) + note, excerpt)
            result = StepResult(label, mode, "warn", rc, elapsed, reason, first, counts)
        else:
            lines = _render_bounded(
                lambda ex: reporter.step_fail(label, rc, elapsed, first, ex) + note, excerpt)
            result = StepResult(label, mode, "fail", rc, elapsed, summary_text, first, counts)
    _emit(lines)
    append_record(result, results)
    return result


# --- self-test ---------------------------------------------------------------

_SELF_TEST_CHILD = (
    "import sys\n"
    "print('self-test line one')\n"
    "print('src/x.cpp:1:1: error: self-test diagnostic')\n"
    "print('self-test line three')\n"
    "sys.exit(3)\n"
)


def _self_test(reporter: Reporter) -> int:
    """Run a tiny failing child and check the RC1 output and the RC3 record."""
    problems: List[str] = []
    with tempfile.TemporaryDirectory(prefix="ci-step-self-test-") as tmp:
        results = Path(tmp) / "results.jsonl"
        captured = io.StringIO()
        with contextlib.redirect_stdout(captured):
            result = run_step("self-test", [sys.executable, "-c", _SELF_TEST_CHILD],
                              patterns=ci_patterns.ALL_SETS, best_effort_reason=None,
                              summary=None, log_dir=Path(tmp) / "logs",
                              reporter=reporter, results_file=results)
        out = captured.getvalue().splitlines()
        if result.exit != 3 or result.outcome != "fail":
            problems.append(f"child exit/outcome {result.exit}/{result.outcome}, expected 3/fail")
        if not _within_ceiling(out):
            problems.append("output exceeds the 64-line/16 KiB ceiling")
        if not out or "self-test diagnostic" not in out[0]:
            problems.append("first line does not carry the first diagnostic")
        for needle in ("self-test line one", "self-test diagnostic", "self-test line three"):
            hits = sum(needle in line for line in out[1:])
            if hits != 1:
                problems.append(f"excerpt shows {needle!r} {hits} times, expected once")
        try:
            records = [json.loads(x) for x in results.read_text(encoding="ascii").splitlines()]
            if len(records) != 1 or records[0].get("exit") != 3 or records[0].get("label") != "self-test":
                problems.append("RC3 record missing or wrong")
        except (OSError, ValueError):
            problems.append("RC3 record unreadable")
    if problems:
        _emit(reporter.step_fail("ci_step self-test", 1, 0.0, problems[0], []))
        return 1
    _emit([f"ok: ci_step self-test ({platform.python_version()}, {sys.platform})"])
    return 0


# --- command line ---------------------------------------------------------------

def _build_parser() -> argparse.ArgumentParser:
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--reporter", choices=BACKEND_NAMES, default=argparse.SUPPRESS,
                        help="reporter backend (default: $CI_REPORTER, else plain)")
    parser = argparse.ArgumentParser(
        prog="ci_step.py",
        description="Run one CI step under the 0014 report contract (RC1-RC3, RC5).",
        parents=[common])
    sub = parser.add_subparsers(dest="command", required=True)
    run = sub.add_parser("run", parents=[common], help="run one command as a CI step")
    run.add_argument("--label", required=True, help="step label")
    run.add_argument("--patterns", default=",".join(ci_patterns.ALL_SETS),
                     help="comma-separated diagnostic pattern sets: "
                          + ", ".join(ci_patterns.ALL_SETS) + " (default: all)")
    run.add_argument("--best-effort", metavar="REASON", default=None,
                     help="best-effort step (RC5): warn on failure, always exit 0")
    run.add_argument("--summary", default=None, help="text appended to the ok: line")
    run.add_argument("--max-diag", type=int, default=MAX_DIAG,
                     help=f"diagnostic lines to show (0-{MAX_DIAG})")
    run.add_argument("--tail", type=int, default=MAX_TAIL,
                     help=f"tail lines to show (0-{MAX_TAIL})")
    run.add_argument("--hygiene", choices=HYGIENE_MODES, default="full",
                     help="counts-only: show CTest status lines only and log under "
                          "$CI_LOG_DIR/live/ (required for live-table steps, RC1)")
    run.add_argument("cmd", nargs=argparse.REMAINDER, help="-- <command> [args...]")
    sub.add_parser("self-test", parents=[common],
                   help="run a built-in failing child and check the runner")
    return parser


def _usage_error(message: str) -> int:
    sys.stderr.write(f"ci_step.py: error: {message}\n")
    return EXIT_USAGE


def _main(argv: Sequence[str], chosen: List[Reporter]) -> int:
    args = _build_parser().parse_args(list(argv))
    try:
        reporter = get_reporter(getattr(args, "reporter", None))
    except ValueError as exc:
        return _usage_error(str(exc))
    chosen.append(reporter)
    if args.command == "self-test":
        return _self_test(reporter)

    cmd = list(args.cmd)
    if cmd and cmd[0] == "--":
        cmd = cmd[1:]
    if not cmd:
        return _usage_error("missing command after --")
    if not _LABEL_OK.match(args.label):
        return _usage_error("label must be 1-80 characters of [A-Za-z0-9 ._/+()-]")
    sets = [s.strip() for s in args.patterns.split(",") if s.strip()]
    unknown = [s for s in sets if s not in ci_patterns.PATTERN_SETS]
    if unknown or not sets:
        return _usage_error(f"unknown pattern set(s): {', '.join(unknown) or '<empty>'}")
    if not 0 <= args.max_diag <= MAX_DIAG or not 0 <= args.tail <= MAX_TAIL:
        return _usage_error(f"--max-diag must be 0-{MAX_DIAG} and --tail 0-{MAX_TAIL} (RC1)")
    log_dir = Path(env_names.get("CI_LOG_DIR", required=True))
    results = Path(env_names.get("CI_RESULTS_FILE", required=True))
    try:
        prepare_log_dir(log_dir)
    except UnsafePathError as exc:
        return _usage_error(f"unsafe log directory: {exc}")
    except OSError as exc:
        return _usage_error(f"cannot create log directory ({exc.strerror})")

    try:
        result = run_step(args.label, cmd, patterns=sets, best_effort_reason=args.best_effort,
                          summary=args.summary, max_diag=args.max_diag, tail=args.tail,
                          log_dir=log_dir, reporter=reporter, results_file=results,
                          hygiene=args.hygiene)
    except UnsafePathError as exc:
        return _usage_error(f"unsafe log path: {exc}")
    if result.mode == "best-effort":
        return 0
    return result.exit


def main(argv: Sequence[str]) -> int:
    """Entry point: parse ``argv`` (without the program name) and run; never raises.

    Returns the process exit code. A runner bug is reported as one
    ``internal error (<exception type>)`` annotation and exit 70, never a traceback.
    """
    try:
        sys.stdout.reconfigure(errors="replace")  # type: ignore[attr-defined]
    except (AttributeError, ValueError):
        pass
    chosen: List[Reporter] = []  # the backend in use, for internal-error reporting
    try:
        return _main(argv, chosen)
    except SystemExit as exc:  # argparse usage errors and env_names exits
        return exc.code if isinstance(exc.code, int) else EXIT_USAGE
    except KeyboardInterrupt:
        return 130
    except Exception as exc:  # noqa: BLE001 - the contract is "never a traceback"
        try:
            backend = chosen[0] if chosen else get_reporter(None)
            lines = backend.internal_error(type(exc).__name__)
        except Exception:  # noqa: BLE001
            lines = [f"error: ci_step: internal error ({type(exc).__name__})"]
        _emit(lines)
        return EXIT_INTERNAL


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
