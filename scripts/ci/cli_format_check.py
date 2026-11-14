#!/usr/bin/env python3
"""Every ``--format`` through the real ``smbios`` CLI, counts only (milestone 0014, Story 03.0 Task 01).

Replaces the inline "Validate every --format on real hardware" block of the old
``ci.yml`` (inventory rows C07, C08, C09) with the same semantics:

- find the ``smbios`` binary under the build tree (``*/smbios/bin/smbios[.exe]``);
  none found is a hard failure;
- Linux only (``--sysfs-info``): one ``info:`` line with the sysfs DMI table
  modes, as ``<name> <mode> <owner>`` triples, at most 3 (C09);
- for each of ``text xml json yaml csv``: run
  ``examples/scripts/check_structured_output.py --format <f> -- [sudo -n] <bin> --format <f>``
  without a shell (C07). A ``0 structures``/``0 rows`` result raises one
  ``empty table`` warning per format; ``text`` (which has no count) is warned
  about when every counted format was empty;
- byte-compare the no-flag output with ``--format text`` in memory, inside a
  child process (``--compare-child``) that prints only the two byte counts (C08);
- success is one line,
  ``ok: cli formats: text non-empty, xml <n>, json <n>, yaml <n>, csv <n>; no-flag == text``.

**Data hygiene** (threat model CR-4, CR-9). Every child reads the live SMBIOS
table, so each runs through ``ci_step.run_step`` with ``hygiene="counts-only"``
and logs under ``$CI_LOG_DIR/live/``. Child text is never relayed: the
checker's ``ok: <fmt>, <N> <unit>`` line is parsed with an anchored regex and
re-rendered from the parsed integer; a ``FAIL: <reason>`` line is shown only
when the reason matches one of the checker's own fixed reason templates
(:data:`FAIL_REASONS`), otherwise a fixed ``check failed (exit <rc>)``. CLI
stdout, CLI stderr and field values are never printed.

Usage::

    python3 scripts/ci/cli_format_check.py --build-dir build/<preset> [--sudo] [--sysfs-info]

Exit codes: 0 pass, 1 a check failed, 2 usage or environment error. Python 3
standard library only.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import argparse  # noqa: E402
import re  # noqa: E402
import stat  # noqa: E402
import subprocess  # noqa: E402
import time  # noqa: E402
from dataclasses import dataclass  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import Final, List, Optional, Pattern, Sequence, Tuple  # noqa: E402

import ci_step  # noqa: E402
import env_names  # noqa: E402
from reporters import Reporter, get_reporter  # noqa: E402

REPO_ROOT: Final = Path(_HERE).resolve().parents[1]
#: The structured-output checker (milestone 0009), run as a child.
CHECK_SCRIPT: Final = REPO_ROOT / "examples" / "scripts" / "check_structured_output.py"
#: The formats checked, in this order (inventory C07).
FORMATS: Final = ("text", "xml", "json", "yaml", "csv")
#: Label of the one ``ok:``/failure line and of the RC3 record.
LABEL: Final = "cli formats"
#: Per-child time limit; a CLI run takes well under a second.
CHILD_TIMEOUT_S: Final = 120.0
#: Where the sysfs DMI tables live (C09).
SYSFS_TABLES: Final = Path("/sys/firmware/dmi/tables")
#: Largest CLI output the compare child holds in memory.
COMPARE_MAX_BYTES: Final = 64 << 20
#: Raw bytes read from the end of a child log to find its result line.
_LOG_TAIL_BYTES: Final = 64 * 1024

#: The checker's success line, fixed against ``check_structured_output.py``:
#: ``ok: <fmt>, <N> structures`` (``rows`` for csv). ``\b``, not ``$``, so a
#: trailing hostile suffix is ignored rather than relayed (the count is
#: re-rendered from the integer).
OK_COUNT_LINE: Final = re.compile(r"^ok: (xml|json|yaml|csv), (\d{1,9}) (structures|rows)\b")
#: The checker's ``text`` success line (text has no count).
OK_TEXT_LINE: Final = re.compile(r"^ok: text, non-empty\b")
#: The compare child's only output line.
COMPARE_LINE: Final = re.compile(r"^compare: (\d{1,12}) (\d{1,12}) (equal|differs)$")

#: The checker's own ``FAIL:`` reasons (``check_structured_output.py``), as
#: anchored templates. Only a reason matching one of these is shown.
FAIL_REASONS: Final[Tuple[Pattern[str], ...]] = tuple(re.compile(p) for p in (
    r"^cannot run command \([A-Za-z0-9 ,.'-]{1,60}\)$",
    r"^command exited with status -?\d{1,5}$",
    r"^stdout is empty$",
    r"^stdout is not valid UTF-8 at byte offset \d{1,12}$",
    r"^invalid JSON at line \d{1,9}, column \d{1,9}$",
    r"^JSON root is not an array$",
    r"^JSON element \d{1,9} is not an object$",
    r"^JSON element \d{1,9} has no string \"type\"$",
    r"^JSON element \d{1,9} has no object \"fields\"$",
    r"^XML contains a <!(?:DOCTYPE|ENTITY) declaration, which is not allowed$",
    r"^invalid XML at line \d{1,9}, column \d{1,9}$",
    r"^invalid CSV at line \d{1,9}$",
    r"^CSV header row is not type,field_name,field_value$",
    r"^CSV row \d{1,9} has \d{1,9} columns, expected 3$",
    r"^YAML document does not start with ---$",
    r"^YAML line \d{1,9} is neither a '- type:' item nor indented under one$",
))

_NAME_TOKEN = re.compile(r"^[A-Za-z0-9._-]{1,40}$")
_PATH_TOKEN = re.compile(r"^[A-Za-z0-9._/+-]{1,120}$")


class _Silent:
    """Reporter proxy that prints nothing: a child's outcome is re-rendered here."""

    def __init__(self, inner: Reporter) -> None:
        self._inner = inner
        self.name = inner.name

    def step_ok(self, *_a, **_k) -> List[str]:  # noqa: D102
        return []

    def step_fail(self, *_a, **_k) -> List[str]:  # noqa: D102
        return []

    def step_warn(self, *_a, **_k) -> List[str]:  # noqa: D102
        return []

    def info(self, *_a, **_k) -> List[str]:  # noqa: D102
        return []

    def __getattr__(self, attr: str):
        return getattr(self._inner, attr)


@dataclass
class _Ctx:
    """Where children log and record, and the backend that renders this script's lines."""

    reporter: Reporter
    log_dir: Path
    results: Path


def _default_ctx(reporter: Optional[Reporter] = None) -> _Ctx:
    return _Ctx(reporter or get_reporter(None),
                Path(env_names.get("CI_LOG_DIR", required=True)),
                Path(env_names.get("CI_RESULTS_FILE", required=True)))


def _show_path(path: Path) -> str:
    """Return ``path`` relative to the workspace if it passes the allowlist, else ``<path>``."""
    try:
        text = path.resolve().relative_to(Path.cwd().resolve()).as_posix()
    except ValueError:
        text = path.name
    return text if _PATH_TOKEN.match(text) else "<path>"


def find_cli(build_dir: Path) -> Optional[Path]:
    """Return the ``smbios`` CLI under ``build_dir`` (``*/smbios/bin/[<config>/]smbios[.exe]``), or ``None``.

    Symlinks are neither followed nor returned; the first match in sorted
    walk order wins, so the result is deterministic.
    """
    if not build_dir.is_dir():
        return None
    for root, dirs, files in os.walk(build_dir, followlinks=False):
        dirs.sort()
        here = Path(root)
        # Single-config generators emit smbios/bin/smbios; Visual Studio adds a
        # per-configuration directory (smbios/bin/Release/smbios.exe).
        direct = here.name == "bin" and here.parent.name == "smbios"
        per_config = here.parent.name == "bin" and here.parent.parent.name == "smbios"
        if not (direct or per_config):
            continue
        for name in sorted(files):
            if name not in ("smbios", "smbios.exe"):
                continue
            candidate = here / name
            try:
                st = os.lstat(candidate)
            except OSError:
                continue
            if stat.S_ISREG(st.st_mode):
                return candidate
    return None


def sysfs_info(tables: Path = SYSFS_TABLES) -> str:
    """Return the C09 ``info:`` line: up to 3 ``<name> <mode> <owner>`` triples.

    Names and owners pass an allowlist (else ``?``); nothing is read from the
    tables themselves, only their metadata.
    """
    try:
        names = sorted(os.listdir(tables))
    except OSError as exc:
        return f"info: sysfs dmi tables: unavailable ({type(exc).__name__})"
    triples: List[str] = []
    for name in names[:3]:
        try:
            st = os.lstat(tables / name)
        except OSError:
            continue
        owner = str(st.st_uid)
        try:
            import pwd  # POSIX only; this line is printed on Linux legs only
            owner = pwd.getpwuid(st.st_uid).pw_name
        except (ImportError, KeyError):
            pass
        shown = name if _NAME_TOKEN.match(name) else "?"
        owner = owner if _NAME_TOKEN.match(owner) else "?"
        triples.append(f"{shown} {stat.S_IMODE(st.st_mode):04o} {owner}")
    return "info: sysfs dmi tables: " + (", ".join(triples) if triples else "empty")


def _last_line(log: Path, pattern: Optional[Pattern[str]] = None) -> str:
    """Return the last non-empty line of ``log`` (or the last one matching ``pattern``).

    Only the final :data:`_LOG_TAIL_BYTES` are read; every line goes through
    ``ci_step.safe_line`` first.
    """
    try:
        with open(log, "rb") as fh:
            fh.seek(0, os.SEEK_END)
            size = fh.tell()
            fh.seek(max(0, size - _LOG_TAIL_BYTES))
            data = fh.read()
    except OSError:
        return ""
    for raw in reversed(data.splitlines()):
        text = ci_step.safe_line(raw, 400).strip()
        if not text:
            continue
        if pattern is None or pattern.match(text):
            return text
    return ""


def _fail_reason(log: Path, rc: int) -> str:
    """Return the checker's own fixed ``FAIL:`` reason, else ``check failed (exit <rc>)``."""
    if rc == ci_step.EXIT_TIMEOUT:
        return f"timed out after {CHILD_TIMEOUT_S:g}s"
    line = _last_line(log, re.compile(r"^FAIL: "))
    reason = line[len("FAIL: "):] if line else ""
    if reason and any(rx.match(reason) for rx in FAIL_REASONS):
        return reason
    return f"check failed (exit {rc})"


def _run_child(label: str, argv: Sequence[str], ctx: _Ctx) -> Tuple[int, Path]:
    """Run one live-table child silently under ``counts-only``; return ``(exit, log path)``."""
    result = ci_step.run_step(label, argv, patterns=["python"], best_effort_reason=None,
                              summary=None, log_dir=ctx.log_dir, reporter=_Silent(ctx.reporter),
                              results_file=ctx.results, timeout_s=CHILD_TIMEOUT_S,
                              hygiene="counts-only")
    return result.exit, ci_step.log_path_for(label, ctx.log_dir / ci_step.LIVE_LOG_SUBDIR)


def _sudo(sudo: bool) -> List[str]:
    # -n: never prompt (ci_step closes stdin); fails fast where sudo needs a password.
    return ["sudo", "-n"] if sudo else []


def check_format(py: str, cli: Path, fmt: str, sudo: bool, *,
                 ctx: Optional[_Ctx] = None) -> Tuple[bool, int, str]:
    """Validate one ``--format`` document; return ``(ok, count, unit)``.

    Args:
        py: Python interpreter that runs the checker (``-I``).
        cli: The ``smbios`` binary.
        fmt: One of :data:`FORMATS`.
        sudo: Run the CLI under ``sudo -n`` (Linux legs).
        ctx: Logging/reporting context; ``None`` reads the neutral names.

    Returns:
        ``(True, n, "structures"|"rows")`` on success; ``(True, -1,
        "non-empty")`` for ``text``; ``(False, -1, <fixed reason>)`` on
        failure, including an unparseable result line.

    Raises:
        ValueError: ``fmt`` is not in :data:`FORMATS`.
    """
    if fmt not in FORMATS:
        raise ValueError(f"unknown format {fmt!r}")
    ctx = ctx or _default_ctx()
    argv = [py, "-I", str(CHECK_SCRIPT), "--format", fmt, "--",
            *_sudo(sudo), str(cli), "--format", fmt]
    rc, log = _run_child(f"cli format {fmt}", argv, ctx)
    if rc != 0:
        return False, -1, _fail_reason(log, rc)
    line = _last_line(log)
    if fmt == "text":
        return (True, -1, "non-empty") if OK_TEXT_LINE.match(line) else \
            (False, -1, "unparseable checker result line")
    match = OK_COUNT_LINE.match(line)
    want_unit = "rows" if fmt == "csv" else "structures"
    if match is None or match.group(1) != fmt or match.group(3) != want_unit:
        return False, -1, "unparseable checker result line"
    return True, int(match.group(2)), want_unit


def noflag_equals_text(cli: Path, sudo: bool, *, py: Optional[str] = None,
                       ctx: Optional[_Ctx] = None) -> Tuple[bool, int, int]:
    """Compare the CLI's no-flag output with ``--format text`` byte-for-byte (C08).

    The comparison runs in a ``--compare-child`` process (under ``ci_step``,
    ``counts-only``) that holds both outputs in memory and prints only their
    sizes, so neither output is printed or written to the workspace.

    Returns:
        ``(equal, no_flag_bytes, text_bytes)``; ``(False, -1, -1)`` when the
        comparison itself could not run.
    """
    ctx = ctx or _default_ctx()
    argv = [py or sys.executable, "-I", str(Path(__file__).resolve()), "--compare-child", "--",
            *_sudo(sudo), str(cli)]
    rc, log = _run_child("cli no-flag compare", argv, ctx)
    match = COMPARE_LINE.match(_last_line(log))
    if rc != 0 or match is None:
        return False, -1, -1
    return match.group(3) == "equal", int(match.group(1)), int(match.group(2))


def _compare_child(cmd: Sequence[str]) -> int:
    """Child mode: run ``cmd`` and ``cmd --format text``; print only the two byte counts."""
    outputs: List[bytes] = []
    for argv in (list(cmd), list(cmd) + ["--format", "text"]):
        try:
            proc = subprocess.run(argv, stdout=subprocess.PIPE, stdin=subprocess.DEVNULL,
                                  check=False, timeout=CHILD_TIMEOUT_S)
        except (OSError, subprocess.TimeoutExpired) as exc:
            ci_step.emit([f"compare-error: {type(exc).__name__}"])
            return 1
        if proc.returncode != 0:
            ci_step.emit([f"compare-error: exit {proc.returncode}"])
            return 1
        if len(proc.stdout) > COMPARE_MAX_BYTES:
            ci_step.emit(["compare-error: output too large"])
            return 1
        outputs.append(proc.stdout)
    verdict = "equal" if outputs[0] == outputs[1] else "differs"
    ci_step.emit([f"compare: {len(outputs[0])} {len(outputs[1])} {verdict}"])
    return 0


def run(build_dir: Path, *, py: Optional[str] = None, sudo: bool = False,
        sysfs: bool = False, ctx: Optional[_Ctx] = None) -> int:
    """Run C09 (optional), C07 and C08; print the RC1 lines; return 0 or 1.

    Args:
        build_dir: The preset's build tree (``build/<preset>``).
        py: Interpreter for the checker; default ``sys.executable``.
        sudo: Run the CLI under ``sudo -n``.
        sysfs: Print the C09 ``info:`` line first.
        ctx: Logging/reporting context; ``None`` reads the neutral names.
    """
    ctx = ctx or _default_ctx()
    reporter = ctx.reporter
    py = py or sys.executable
    start = time.monotonic()
    if sysfs:
        ci_step.emit(reporter.info(sysfs_info()))
    cli = find_cli(build_dir)
    if cli is None:
        return _fail(ctx, start, f"smbios CLI binary not found under {_show_path(build_dir)}", [])

    counts = {}
    failures: List[str] = []
    for fmt in FORMATS:
        ok, count, unit = check_format(py, cli, fmt, sudo, ctx=ctx)
        if ok:
            counts[fmt] = count
        else:
            failures.append(f"smbios --format {fmt}: {unit}")
    equal, a_bytes, b_bytes = noflag_equals_text(cli, sudo, py=py, ctx=ctx)
    if a_bytes < 0:
        failures.append("no-flag output: comparison could not run")
    elif not equal:
        failures.append(f"no-flag output: expected == --format text -> actual differs "
                        f"({a_bytes} vs {b_bytes} bytes)")
    if failures:
        return _fail(ctx, start, failures[0], failures[1:])

    counted = [f for f in FORMATS if f != "text"]
    warnings: List[str] = []
    for fmt in FORMATS:
        empty = (all(counts[f] == 0 for f in counted) if fmt == "text" else counts[fmt] == 0)
        if empty:
            warnings += reporter.warning(
                "empty table",
                f"smbios --format {fmt}: well-formed but empty document - this runner "
                f"exposed no readable SMBIOS table, so {fmt} was validated for shape only")
    summary = ", ".join(f"{fmt} {'non-empty' if fmt == 'text' else counts[fmt]}"
                        for fmt in FORMATS) + "; no-flag == text"
    elapsed = round(time.monotonic() - start, 1)
    ci_step.emit(warnings + reporter.step_ok(LABEL, summary, elapsed))
    ci_step.append_record(ci_step.StepResult(LABEL, "hard", "ok", 0, elapsed, summary, None,
                                             None), ctx.results)
    return 0


def _fail(ctx: _Ctx, start: float, first: str, rest: List[str]) -> int:
    elapsed = round(time.monotonic() - start, 1)
    ci_step.emit(ctx.reporter.step_fail(LABEL, 1, elapsed, first, rest[:ci_step.MAX_DIAG]))
    ci_step.append_record(ci_step.StepResult(LABEL, "hard", "fail", 1, elapsed, "", first, None),
                          ctx.results)
    return 1


def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="cli_format_check.py",
                                description="Validate every smbios --format, counts only.")
    p.add_argument("--build-dir", type=Path, help="build tree of the preset (build/<preset>)")
    p.add_argument("--py", default=None, help="interpreter for the checker (default: this one)")
    p.add_argument("--sudo", action="store_true", help="run the CLI under sudo -n (Linux)")
    p.add_argument("--sysfs-info", action="store_true", help="print the C09 sysfs mode line")
    p.add_argument("--reporter", default=None)
    p.add_argument("--compare-child", action="store_true", help=argparse.SUPPRESS)
    p.add_argument("cmd", nargs=argparse.REMAINDER, help=argparse.SUPPRESS)
    return p


def main(argv: Sequence[str]) -> int:
    """Command-line entry point; returns the exit code (never a traceback)."""
    args = _parser().parse_args(list(argv))
    if args.compare_child:
        cmd = list(args.cmd)
        if cmd and cmd[0] == "--":
            cmd = cmd[1:]
        if not cmd:
            ci_step.emit_error("cli_format_check.py: error: --compare-child needs a command")
            return env_names.EXIT_ENV_ERROR
        return _compare_child(cmd)
    if args.build_dir is None:
        ci_step.emit_error("cli_format_check.py: error: --build-dir is required")
        return env_names.EXIT_ENV_ERROR
    try:
        reporter = get_reporter(args.reporter)
    except ValueError as exc:
        ci_step.emit_error(f"cli_format_check.py: error: {exc}")
        return env_names.EXIT_ENV_ERROR
    return run(args.build_dir, py=args.py, sudo=args.sudo, sysfs=args.sysfs_info,
               ctx=_default_ctx(reporter))


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step.emit([f"error: cli_format_check.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
