#!/usr/bin/env python3
"""Run one named ctest set and report it from JUnit counts (milestone 0014, Story 03.0 Task 02).

Plan E4: the ``% tests passed`` summary line changed between ctest 3.x and
4.x and broke a gate. Here **the exit code decides** and the counts come from
``ctest --output-junit``; no regex in this file reads ctest's human output.

Run (one ``ci_step`` sub-step, RC1-RC3)::

    ctest --test-dir <dir> [-C <config>] <set args> [--output-on-failure]
          --output-junit $CI_TEMP_DIR/junit-<label>.xml --timeout 60 --no-tests=error

The sets live in ``test_sets.json`` (``unprivileged`` C06/C13, ``live_table``
C10/C14, ``gui_ci`` C19). Only non-live sets add ``--output-on-failure``.

Report:

- success: ``ok: tests <label>: <p> passed, <s> skipped, <n> total (<t>s)``;
- failure: the RC1 failure (annotation + excerpt), led by one discrepancy line
  per failing test, ``<test>: expected passed -> <failed|timeout|notrun>``, at
  most 20 plus ``(+<k> more)``;
- a missing, oversized, unreadable or DTD-bearing JUnit file: the exit code
  alone decides and the summary says ``counts unavailable``.

Counts go into the step's RC3 record (``counts``). JUnit ``system-out`` (test
stdout) is never read, printed or recorded.

**Data hygiene** (RC1, threat model CR-9, CR-13). A ``live_table`` set reads
the real SMBIOS table: it runs under ``ci_step`` ``hygiene="counts-only"``
(only CTest status lines can be shown; the log goes to ``CI_LOG_DIR/live/``)
and never with ``--output-on-failure``; its JUnit file stays in
``$CI_TEMP_DIR``, which no upload path names. ``parse_junit`` refuses any
input containing ``<!DOCTYPE`` or ``<!ENTITY`` (entity expansion), a
symlink, a non-regular file, or more than 32 MiB. Test names in discrepancy
lines must match ``^[A-Za-z0-9_./:-]{1,120}$``, else ``<test #n>``.

Usage::

    python3 scripts/ci/ctest_report.py --set <name> --test-dir <dir> [--config <cfg>]
        [--label <label>] [--best-effort "<reason>"] [--sudo] [--reporter <backend>]

``--sudo`` runs ``sudo -n <absolute ctest>`` (plan Q4, Linux build leg).

Exit codes: ctest's (0 pass, non-zero fail; 0 always for ``--best-effort``),
2 usage or environment error, 70 internal error. Python 3.9+ standard library.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import argparse  # noqa: E402
import json  # noqa: E402
import re  # noqa: E402
import shutil  # noqa: E402
import stat  # noqa: E402
import xml.etree.ElementTree as ET  # noqa: E402
from dataclasses import dataclass  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import Dict, Final, List, Optional, Sequence, Tuple  # noqa: E402

import ci_step  # noqa: E402
import env_names  # noqa: E402
from reporters import Reporter, get_reporter  # noqa: E402

TEST_SETS_FILE: Final = Path(_HERE) / "test_sets.json"
#: Largest JUnit file parsed (threat model CR-13).
JUNIT_MAX_BYTES: Final = 32 << 20
#: Bytes searched for a DTD before parsing; the whole (capped) file is searched.
_DTD_MARKERS: Final = (b"<!DOCTYPE", b"<!ENTITY")
#: Discrepancy lines shown (RC1 discrepancy form).
MAX_DISCREPANCIES: Final = 20
#: Summary text when no count could be read.
COUNTS_UNAVAILABLE: Final = "counts unavailable"
#: ctest selection flags a set may use (anything else in test_sets.json is refused).
_SELECTION_FLAGS: Final = ("-R", "-E", "-L", "-LE")

_TEST_NAME: Final = re.compile(r"^[A-Za-z0-9_./:-]{1,120}$")
_LABEL: Final = re.compile(r"^[a-z0-9][a-z0-9-]{0,39}$")
_SET_NAME: Final = re.compile(r"^[a-z0-9_]{1,40}$")
_CONFIG: Final = re.compile(r"^[A-Za-z0-9_]{1,40}$")


class UsageError(Exception):
    """A usage or environment error (exit 2)."""


@dataclass(frozen=True)
class TestCounts:
    """Per-status test counts from one JUnit file (``timeout`` counts as ``failed``)."""

    __test__ = False  # not a unittest class

    passed: int
    failed: int
    skipped: int
    not_run: int
    total: int

    def as_record(self) -> Dict[str, int]:
        """Return the RC3 ``counts`` object."""
        return {"passed": self.passed, "failed": self.failed, "skipped": self.skipped,
                "not_run": self.not_run}


@dataclass(frozen=True)
class TestSet:
    """One ``test_sets.json`` entry."""

    __test__ = False

    name: str
    args: Tuple[str, ...]
    output_on_failure: bool
    live_table: bool


# ---------------------------------------------------------------------------
# test_sets.json
# ---------------------------------------------------------------------------

def load_sets(path: Path = TEST_SETS_FILE) -> Tuple[str, Dict[str, TestSet]]:
    """Return ``(timeout, {name: TestSet})`` from ``test_sets.json``.

    Raises:
        UsageError: The file is unreadable or a set is malformed (a selection
            flag outside ``-R``/``-E``/``-L``/``-LE``, a non-string value).
    """
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        raise UsageError(f"cannot read {path.name}") from None
    timeout = data.get("timeout") if isinstance(data, dict) else None
    sets = data.get("sets") if isinstance(data, dict) else None
    if not (isinstance(timeout, str) and timeout.isdigit() and isinstance(sets, dict)):
        raise UsageError(f"{path.name}: expected a digit-string 'timeout' and a 'sets' object")
    out: Dict[str, TestSet] = {}
    for name, entry in sets.items():
        args = entry.get("args") if isinstance(entry, dict) else None
        if not (_SET_NAME.match(name) and isinstance(args, list) and len(args) % 2 == 0
                and all(isinstance(a, str) and a for a in args)
                and all(args[i] in _SELECTION_FLAGS for i in range(0, len(args), 2))
                and isinstance(entry.get("output_on_failure"), bool)
                and isinstance(entry.get("live_table"), bool)):
            raise UsageError(f"{path.name}: set entries need flag/value 'args' pairs and "
                             "boolean 'output_on_failure'/'live_table'")
        out[name] = TestSet(name, tuple(args), entry["output_on_failure"], entry["live_table"])
    return timeout, out


# ---------------------------------------------------------------------------
# JUnit parsing
# ---------------------------------------------------------------------------

def _classify(case: ET.Element) -> str:
    """Map one ctest ``<testcase>`` to passed/failed/timeout/skipped/notrun.

    ctest writes ``status="run|fail|notrun|disabled"``. A ``fail`` whose
    ``<failure message>`` is ``Timeout`` is a timeout; a ``notrun`` whose
    ``<skipped message>`` starts with ``SKIP_`` (``SKIP_RETURN_CODE``,
    ``SKIP_REGULAR_EXPRESSION_MATCHED``) is a deliberate skip, any other
    ``notrun`` (a missing required file, a fixture failure) counts as not run;
    ``disabled`` is skipped. These are attribute values, not summary text.
    An unknown status counts as failed (fail closed).
    """
    status = case.get("status", "")
    if status == "run":
        return "passed"
    if status == "disabled":
        return "skipped"
    if status == "notrun":
        skipped = case.find("skipped")
        message = skipped.get("message", "") if skipped is not None else ""
        return "skipped" if message.startswith("SKIP_") else "notrun"
    failure = case.find("failure")
    if failure is not None and failure.get("message", "") == "Timeout":
        return "timeout"
    return "failed"


def parse_junit(path: Path, max_bytes: int = JUNIT_MAX_BYTES
                ) -> Optional[Tuple[TestCounts, List[Tuple[str, str]]]]:
    """Return ``(counts, [(test name, status), ...] for every non-passing test)``, or ``None``.

    ``None`` means "counts unavailable": ``path`` is missing, a symlink, not a
    regular file, larger than ``max_bytes``, holds a ``<!DOCTYPE``/``<!ENTITY``
    (refused before parsing, CR-13), is not well-formed, or has no
    ``testsuite`` root. Status is ``failed``, ``timeout`` or ``notrun``
    (skips are not discrepancies). Names failing the allowlist become
    ``<test #n>`` (``n`` is the 1-based position in the file). ``system-out``
    is never read.
    """
    try:
        st = os.lstat(path)
        if not stat.S_ISREG(st.st_mode) or st.st_size > max_bytes:
            return None
        with open(path, "rb") as fh:
            data = fh.read(max_bytes + 1)
    except OSError:
        return None
    if len(data) > max_bytes or any(marker in data for marker in _DTD_MARKERS):
        return None
    try:
        root = ET.fromstring(data)
    except (ET.ParseError, ValueError):
        return None
    if root.tag not in ("testsuite", "testsuites"):
        return None
    tally = {"passed": 0, "failed": 0, "timeout": 0, "skipped": 0, "notrun": 0}
    bad: List[Tuple[str, str]] = []
    for n, case in enumerate(root.iter("testcase"), start=1):
        kind = _classify(case)
        tally[kind] += 1
        if kind in ("failed", "timeout", "notrun"):
            name = case.get("name", "")
            bad.append((name if _TEST_NAME.match(name) else f"<test #{n}>", kind))
    counts = TestCounts(passed=tally["passed"], failed=tally["failed"] + tally["timeout"],
                        skipped=tally["skipped"], not_run=tally["notrun"],
                        total=sum(tally.values()))
    return counts, bad


def discrepancy_lines(bad: Sequence[Tuple[str, str]]) -> List[str]:
    """Return the RC1 discrepancy lines, at most :data:`MAX_DISCREPANCIES` plus ``(+k more)``."""
    lines = [f"{name}: expected passed -> {status}"[:ci_step.SUMMARY_MAX]
             for name, status in bad[:MAX_DISCREPANCIES]]
    if len(bad) > MAX_DISCREPANCIES:
        lines.append(f"(+{len(bad) - MAX_DISCREPANCIES} more)")
    return lines


def summarize(parsed: Optional[Tuple[TestCounts, List[Tuple[str, str]]]]) -> ci_step.StepFinish:
    """Turn a :func:`parse_junit` result into the step's summary, counts and excerpt lead."""
    if parsed is None:
        return ci_step.StepFinish(summary=COUNTS_UNAVAILABLE, counts=None,
                                  lines=(f"info: {COUNTS_UNAVAILABLE} (JUnit missing or "
                                         "unreadable); the exit code decided",))
    counts, bad = parsed
    summary = f"{counts.passed} passed, {counts.skipped} skipped, {counts.total} total"
    lines = discrepancy_lines(bad)
    return ci_step.StepFinish(summary=summary, counts=counts.as_record(),
                              first=lines[0] if lines else None, lines=tuple(lines))


# ---------------------------------------------------------------------------
# Running a set
# ---------------------------------------------------------------------------

def junit_path(temp_dir: Path, label: str) -> Path:
    """Return ``$CI_TEMP_DIR/junit-<label>.xml`` (CR-13: a neutral name, never the platform's)."""
    return temp_dir / f"junit-{label}.xml"


def _clear_stale(path: Path) -> None:
    """Remove a previous run's JUnit file so stale counts can never be reported.

    Raises:
        ci_step.UnsafePathError: ``path`` is a symlink or a directory.
    """
    if path.is_symlink() or path.is_dir():
        raise ci_step.UnsafePathError(f"refusing JUnit path {path.name!r}")
    try:
        path.unlink()
    except FileNotFoundError:
        pass


def ctest_argv(test_set: TestSet, timeout: str, test_dir: Path, config: Optional[str],
               junit: Path, *, sudo: bool = False) -> List[str]:
    """Return the ctest argv for ``test_set`` (no shell).

    With ``sudo`` the argv is ``sudo -n <absolute ctest> ...``: ``sudo``'s
    ``secure_path`` would otherwise miss the job-local tools venv.
    """
    ctest = "ctest"
    prefix: List[str] = []
    if sudo:
        ctest = shutil.which("ctest") or "ctest"
        prefix = ["sudo", "-n"]  # -n: never prompt (ci_step closes stdin)
    argv = prefix + [ctest, "--test-dir", str(test_dir)]
    if config:
        argv += ["-C", config]
    argv += list(test_set.args)
    if test_set.output_on_failure and not test_set.live_table:
        argv.append("--output-on-failure")
    return argv + ["--output-junit", str(junit), "--timeout", timeout, "--no-tests=error"]


def run_set(set_name: str, test_dir: Path, config: Optional[str], label: str,
            best_effort_reason: Optional[str], *, sudo: bool = False,
            reporter: Optional[Reporter] = None, log_dir: Optional[Path] = None,
            results: Optional[Path] = None, temp_dir: Optional[Path] = None,
            sets_file: Path = TEST_SETS_FILE) -> int:
    """Run ``set_name`` through ``ci_step`` and report it from JUnit; return the step exit.

    Args:
        set_name: A ``test_sets.json`` set.
        test_dir: The build tree (``ctest --test-dir``).
        config: ``-C`` value (multi-config generators), or ``None``/empty.
        label: Step label suffix and JUnit file stem: the step is ``tests <label>``.
        best_effort_reason: Non-``None`` makes the step best-effort (RC5); it
            then returns 0 and a failure is a ``warn``.
        sudo: Run ctest under ``sudo -n`` (plan Q4).
        reporter, log_dir, results, temp_dir: Default to the neutral names.

    Raises:
        UsageError: Unknown set, malformed label/config, or unreadable sets file.
        ci_step.UnsafePathError: The JUnit path or log directory is unsafe.
    """
    if not _LABEL.match(label):
        raise UsageError("label must match [a-z0-9][a-z0-9-]*")
    if config and not _CONFIG.match(config):
        raise UsageError("config must match [A-Za-z0-9_]+")
    timeout, sets = load_sets(sets_file)
    if set_name not in sets:
        raise UsageError(f"set must be one of: {', '.join(sorted(sets))}")
    test_set = sets[set_name]
    temp_dir = temp_dir or Path(env_names.get("CI_TEMP_DIR", required=True))
    junit = junit_path(temp_dir, label)
    _clear_stale(junit)
    argv = ctest_argv(test_set, timeout, test_dir, config or None, junit, sudo=sudo)
    result = ci_step.run_step(
        f"tests {label}", argv, patterns=["ctest", "boost", "sanitizer"],
        best_effort_reason=best_effort_reason, summary=None,
        log_dir=log_dir or Path(env_names.get("CI_LOG_DIR", required=True)),
        reporter=reporter, results_file=results,
        hygiene="counts-only" if test_set.live_table else "full",
        finish=lambda _rc: summarize(parse_junit(junit)))
    return 0 if result.mode == "best-effort" else result.exit


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------

def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="ctest_report.py",
                                description="Run one ctest set; report from JUnit counts.")
    p.add_argument("--set", dest="set_name", required=True, help="a test_sets.json set")
    p.add_argument("--test-dir", type=Path, required=True, help="build tree")
    p.add_argument("--config", default=None, help="ctest -C value")
    p.add_argument("--label", default=None, help="step label suffix (default: the set name)")
    p.add_argument("--best-effort", default=None, metavar="REASON")
    p.add_argument("--sudo", action="store_true", help="run ctest under sudo -n (Q4)")
    p.add_argument("--reporter", default=None, help="github|teamcity|plain")
    return p


def main(argv: Sequence[str]) -> int:
    """Command-line entry point; returns the exit code (never a traceback)."""
    args = _parser().parse_args(list(argv))
    try:
        try:
            reporter = get_reporter(args.reporter)
        except ValueError:
            raise UsageError("reporter must be one of: github, teamcity, plain") from None
        label = args.label or args.set_name.replace("_", "-")
        return run_set(args.set_name, args.test_dir, args.config, label, args.best_effort,
                       sudo=args.sudo, reporter=reporter)
    except UsageError as exc:
        ci_step.emit_error(f"ctest_report.py: error: {exc}")
        return env_names.EXIT_ENV_ERROR
    except ci_step.UnsafePathError as exc:
        ci_step.emit_error(f"ctest_report.py: error: unsafe path ({exc})")
        return env_names.EXIT_ENV_ERROR


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except KeyboardInterrupt:
        sys.exit(130)
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step.emit([f"error: ctest_report.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
