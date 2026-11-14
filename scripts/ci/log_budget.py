#!/usr/bin/env python3
"""Measure a workflow run's log budget against the baseline (milestone 0014, Story 03.0 Task 04).

Usage::

    python3 scripts/ci/log_budget.py <run-id> [--repo <owner/name>] [--gh <token>]...

Calls ``gh run view <run-id> --json jobs`` and, per job,
``gh run view <run-id> --log --job <job-id>`` (argv lists, never a shell;
``--gh`` replaces the ``gh`` command, so tests substitute a stub) and prints,
per job, **counts only** - never a log line:

- total bytes and lines of the job log;
- the split by the log's step-name column: project-owned lines (steps that
  are ``run:`` steps in ``.github/workflows/*.yml``), the runner's echo of
  those steps' command and ``env:`` block (``##[group]Run`` ... ``##[endgroup]``),
  and third-party lines (actions, ``Set up job``, ``Post ...``). A log without
  a usable step column says ``split unavailable`` (status.md 2026-11-13: the
  older runs' Linux legs);
- the longest failing-step block (lines, bytes) when a step failed.

Then it checks the Task 04 targets (``log_baseline.json`` ``targets``):

- a failing (or warning) project step: at most 64 lines and 16 KiB (RC1);
- a green project step: no line other than an RC line (``ok:``, ``skip:``,
  ``info:``, ``env:``, ``output:``); the runner echo is not counted;
- a green job: fewer total bytes than the baseline job it replaces
  (``log_baseline.json`` ``jobs``/``leg_map``; the old run's job names match
  directly). Third-party output is reported, never counted against a target.

Discrepancies use the RC1 form ``<job>[/<step>]: expected <x> -> actual <y>``,
at most 20 plus ``(+<k> more)``. Exit codes: 0 within budget, 1 a target
missed, 2 usage error or ``gh`` failure. Python 3.9+ standard library only.
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
import subprocess  # noqa: E402
from dataclasses import dataclass, field  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import Any, Callable, Dict, Final, List, Mapping, Optional, Pattern, Sequence, Tuple  # noqa: E402

import check_workflow_policy as policy_gate  # noqa: E402
import ci_step  # noqa: E402

REPO_ROOT: Final = Path(_HERE).resolve().parents[1]
BASELINE_FILE: Final = Path(_HERE) / "log_baseline.json"
WORKFLOWS_DIR: Final = REPO_ROOT / ".github" / "workflows"

MAX_LINES: Final = 20
LINE_WIDTH: Final = 160
GH_TIMEOUT_S: Final = 180.0
#: Largest single gh output read (a job log of the old workflow is about 0.5 MB).
GH_MAX_BYTES: Final = 256 << 20
#: Share of lines that must carry a usable step column for the split to be reported.
SPLIT_MIN_SHARE: Final = 0.9
#: The step column gh prints when it cannot attribute a line.
UNKNOWN_STEP: Final = "UNKNOWN STEP"
#: Line prefixes allowed in a green project step: RC1 ok/skip, RC4 env, info, plain-reporter output.
RC_PREFIXES: Final = ("ok: ", "skip: ", "info: ", "env: ", "output: ")
#: Rendered annotation prefixes in a downloaded log (a warning marks a best-effort step).
WARNING_PREFIX: Final = "##[warning]"
ERROR_PREFIX: Final = "##[error]"

EXIT_OK: Final = 0
EXIT_TARGET: Final = 1
EXIT_USAGE: Final = 2

_ID = re.compile(r"^[0-9]{1,20}$")
_REPO = re.compile(r"^[A-Za-z0-9_.-]{1,100}/[A-Za-z0-9_.-]{1,100}$")
_TIMESTAMP = re.compile(r"^\ufeff?\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d+)?Z ?")
_LEG = re.compile(r"\(([a-z0-9-]{1,32})\)\s*$")
_NAME = re.compile(r"^[A-Za-z0-9 ._/(),+-]{1,100}$")
_EXPR = re.compile(r"\$\{\{.*?\}\}")

Runner = Callable[[Sequence[str]], bytes]


class GhError(Exception):
    """``gh`` failed or returned something unusable (exit 2). Never carries gh's output."""


def safe_name(name: Any) -> str:
    """Return ``name`` if it is a plain job/step name, else ``<name>`` (no hostile text relayed)."""
    text = str(name)
    return text if _NAME.match(text) else "<name>"


# ---------------------------------------------------------------------------
# gh access
# ---------------------------------------------------------------------------

def subprocess_runner(gh: Sequence[str]) -> Runner:
    """Return a :data:`Runner` that runs ``gh <args>`` without a shell and returns stdout."""
    def run(args: Sequence[str]) -> bytes:
        try:
            proc = subprocess.run([*gh, *args], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                  stdin=subprocess.DEVNULL, timeout=GH_TIMEOUT_S, check=False)
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise GhError(f"cannot run gh ({type(exc).__name__})") from None
        if proc.returncode != 0:
            raise GhError(f"gh exited {proc.returncode}")
        if len(proc.stdout) > GH_MAX_BYTES:
            raise GhError("gh output too large")
        return proc.stdout
    return run


def _repo_args(repo: Optional[str]) -> List[str]:
    return ["--repo", repo] if repo else []


def fetch_jobs(run_id: str, runner: Runner, repo: Optional[str] = None) -> List[Dict[str, Any]]:
    """Return the run's jobs from ``gh run view <id> --json jobs``.

    Raises:
        GhError: gh failed or the JSON has no ``jobs`` list.
    """
    raw = runner(["run", "view", run_id, "--json", "jobs", *_repo_args(repo)])
    try:
        jobs = json.loads(raw.decode("utf-8"))["jobs"]
    except (UnicodeDecodeError, ValueError, KeyError, TypeError):
        raise GhError("gh run view --json jobs: no jobs list") from None
    if not isinstance(jobs, list):
        raise GhError("gh run view --json jobs: no jobs list")
    return [j for j in jobs if isinstance(j, dict)]


def fetch_log(run_id: str, job_id: str, runner: Runner, repo: Optional[str] = None) -> bytes:
    """Return one job's log from ``gh run view <id> --log --job <job-id>``."""
    return runner(["run", "view", run_id, "--log", "--job", job_id, *_repo_args(repo)])


# ---------------------------------------------------------------------------
# Project-owned step names
# ---------------------------------------------------------------------------

def project_step_patterns(workflows_dir: Path = WORKFLOWS_DIR) -> List[Pattern[str]]:
    """Return one anchored pattern per ``run:`` step name in the workflows (an expression becomes ``.*``)."""
    out: List[Pattern[str]] = []
    for path in sorted(workflows_dir.glob("*.yml")):
        try:
            root = policy_gate.load_yaml(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, policy_gate.YamlSubsetError):
            continue
        jobs = root.get("jobs") if isinstance(root, dict) else None
        for job in (jobs or {}).values():
            steps = job.get("steps") if isinstance(job, dict) else None
            for step in steps or []:
                if isinstance(step, dict) and "run" in step and isinstance(step.get("name"), str):
                    parts = _EXPR.split(step["name"])
                    out.append(re.compile("^" + ".*".join(re.escape(p) for p in parts) + "$"))
    return out


def is_project_step(name: str, patterns: Sequence[Pattern[str]]) -> bool:
    """Whether the step called ``name`` is a project-owned ``run:`` step."""
    return any(p.match(name) for p in patterns)


# ---------------------------------------------------------------------------
# Measurement
# ---------------------------------------------------------------------------

@dataclass
class StepCount:
    """Counts of one step's lines (runner echo kept apart)."""

    lines: int = 0
    bytes: int = 0
    echo: int = 0
    extra: int = 0          # non-RC lines (green-step rule)
    warned: bool = False
    errored: bool = False


@dataclass
class JobMeasure:
    """Counts of one job's log; never any log text."""

    name: str
    conclusion: str
    bytes: int
    lines: int
    split: bool
    project: int = 0
    echo: int = 0
    third_party: int = 0
    steps: Dict[str, StepCount] = field(default_factory=dict)
    project_steps: List[str] = field(default_factory=list)
    failed_steps: List[str] = field(default_factory=list)

    def longest_failing(self) -> Optional[Tuple[str, StepCount]]:
        """Return the failing project step with the most lines, if any."""
        found = [(s, self.steps[s]) for s in self.failed_steps if s in self.steps]
        return max(found, key=lambda x: (x[1].lines, x[1].bytes)) if found else None


def split_line(line: str) -> Tuple[Optional[str], str]:
    """Return ``(step or None, text)`` of one ``gh --log`` line (``<job>\\t<step>\\t<ts> <text>``)."""
    parts = line.split("\t", 2)
    if len(parts) != 3:
        return None, _TIMESTAMP.sub("", line)
    step = parts[1].strip()
    return (step if step and step != UNKNOWN_STEP else None), _TIMESTAMP.sub("", parts[2])


def measure(job: Mapping[str, Any], raw: bytes, patterns: Sequence[Pattern[str]]) -> JobMeasure:
    """Count one job's log: totals, the project/echo/third-party split, per-step counts."""
    text = raw.decode("utf-8", errors="replace")
    lines = text.splitlines()
    m = JobMeasure(safe_name(job.get("name", "?")), str(job.get("conclusion") or "?"),
                   len(raw), len(lines), split=False)
    failed = {str(s.get("name")) for s in job.get("steps") or []
              if isinstance(s, dict) and s.get("conclusion") == "failure"}
    parsed = [split_line(line) for line in lines]
    with_step = sum(1 for step, _ in parsed if step is not None)
    m.split = bool(lines) and with_step >= SPLIT_MIN_SHARE * len(lines)
    if not m.split:
        return m
    in_echo: Dict[str, bool] = {}
    project_cache: Dict[str, bool] = {}
    for (step, body), line in zip(parsed, lines):
        if step is None:
            m.third_party += 1
            continue
        if step not in project_cache:
            project_cache[step] = is_project_step(step, patterns)
            if project_cache[step]:
                m.project_steps.append(step)
        if not project_cache[step]:
            m.third_party += 1
            continue
        count = m.steps.setdefault(step, StepCount())
        if body.startswith("##[group]Run "):
            in_echo[step] = True
        if in_echo.get(step):
            count.echo += 1
            m.echo += 1
            if body.startswith("##[endgroup]"):
                in_echo[step] = False
            continue
        count.lines += 1
        count.bytes += len(line.encode("utf-8")) + 1
        m.project += 1
        if body.startswith(WARNING_PREFIX):
            count.warned = True
        elif body.startswith(ERROR_PREFIX):
            count.errored = True
        elif not body.startswith(RC_PREFIXES) and body.strip():
            count.extra += 1
    m.failed_steps = [safe_name(s) for s in m.project_steps if s in failed]
    m.steps = {safe_name(k): v for k, v in m.steps.items()}
    m.project_steps = [safe_name(s) for s in m.project_steps]
    return m


def baseline_for(name: str, baseline: Mapping[str, Any]) -> Optional[Tuple[str, int]]:
    """Return ``(baseline job, bytes)`` for job ``name`` (old name, or a new leg via ``leg_map``)."""
    jobs = baseline.get("jobs", {})
    if name in jobs:
        return name, int(jobs[name]["bytes"])
    match = _LEG.search(name)
    old = baseline.get("leg_map", {}).get(match.group(1)) if match else None
    if old and old in jobs:
        return old, int(jobs[old]["bytes"])
    return None


def check_targets(m: JobMeasure, baseline: Mapping[str, Any]) -> List[str]:
    """Return the discrepancy lines of one job against the Task 04 targets."""
    targets = baseline.get("targets", {})
    max_lines = int(targets.get("failing_step_max_lines", 64))
    max_bytes = int(targets.get("failing_step_max_bytes", 16384))
    max_extra = int(targets.get("green_extra_lines_per_project_step", 0))
    out: List[str] = []
    for step in m.project_steps:
        c = m.steps.get(step)
        if c is None:
            continue
        bounded = step in m.failed_steps or c.warned or c.errored
        if bounded:
            if c.lines > max_lines:
                out.append(f"{m.name}/{step}: expected <= {max_lines} lines -> actual {c.lines}")
            if c.bytes > max_bytes:
                out.append(f"{m.name}/{step}: expected <= {max_bytes} bytes -> actual {c.bytes}")
        elif c.extra > max_extra:
            out.append(f"{m.name}/{step}: expected <= {max_extra} non-RC lines -> "
                       f"actual {c.extra}")
    if m.conclusion == "success":
        base = baseline_for(m.name, baseline)
        if base is not None and m.bytes >= base[1]:
            out.append(f"{m.name}: expected bytes < {base[1]} ({base[0]}) -> actual {m.bytes}")
    return out


def summary_line(m: JobMeasure, baseline: Mapping[str, Any]) -> str:
    """Return the one counts-only line for a job."""
    parts = [f"{m.name} [{safe_name(m.conclusion)}]: {m.bytes} B, {m.lines} lines"]
    if m.split:
        parts.append(f"project {m.project}, runner echo {m.echo}, third-party {m.third_party}")
        worst = m.longest_failing()
        if worst is not None:
            parts.append(f"longest failing step {worst[0]}: {worst[1].lines} lines, "
                         f"{worst[1].bytes} B")
    else:
        parts.append("split unavailable")
    base = baseline_for(m.name, baseline)
    if base is not None:
        parts.append(f"baseline {base[1]} B")
    return "; ".join(parts)


def bounded(lines: Sequence[str]) -> List[str]:
    """Cut to the RC1 discrepancy bound: 20 lines of at most 160 characters, then ``(+k more)``."""
    out = [line[:LINE_WIDTH] for line in lines[:MAX_LINES]]
    if len(lines) > MAX_LINES:
        out.append(f"(+{len(lines) - MAX_LINES} more)")
    return out


def load_baseline(path: Path = BASELINE_FILE) -> Dict[str, Any]:
    """Return the parsed baseline file.

    Raises:
        ValueError: Unreadable, or no ``jobs`` object.
    """
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        raise ValueError(f"cannot read {path.name}") from None
    if not isinstance(data, dict) or not isinstance(data.get("jobs"), dict):
        raise ValueError(f"{path.name}: expected a 'jobs' object")
    return data


def run(run_id: str, runner: Runner, *, repo: Optional[str] = None,
        baseline: Optional[Mapping[str, Any]] = None,
        patterns: Optional[Sequence[Pattern[str]]] = None) -> Tuple[List[str], List[str]]:
    """Measure every job of ``run_id``; return ``(summary lines, discrepancy lines)``.

    Raises:
        GhError: A gh call failed.
    """
    baseline = baseline if baseline is not None else load_baseline()
    patterns = patterns if patterns is not None else project_step_patterns()
    summaries: List[str] = []
    problems: List[str] = []
    for job in fetch_jobs(run_id, runner, repo):
        job_id = str(job.get("databaseId", ""))
        if not _ID.match(job_id):
            problems.append(f"{safe_name(job.get('name', '?'))}: expected a job id -> actual none")
            continue
        m = measure(job, fetch_log(run_id, job_id, runner, repo), patterns)
        summaries.append(summary_line(m, baseline))
        problems += check_targets(m, baseline)
    return summaries, problems


def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="log_budget.py", description="Measure a run's log budget.")
    p.add_argument("run_id", help="workflow run id (digits)")
    p.add_argument("--repo", default=None, help="owner/name (default: gh's current repository)")
    p.add_argument("--gh", action="append", default=None,
                   help="gh command token (repeat for several; default: gh)")
    p.add_argument("--baseline", default=str(BASELINE_FILE), help="baseline data file")
    return p


def main(argv: Sequence[str]) -> int:
    """Measure, print counts and discrepancies; return 0, 1 or 2."""
    args = _parser().parse_args(list(argv))
    if not _ID.match(args.run_id) or (args.repo and not _REPO.match(args.repo)):
        ci_step.emit_error("log_budget.py: error: run id must be digits, --repo owner/name")
        return EXIT_USAGE
    try:
        baseline = load_baseline(Path(args.baseline))
        summaries, problems = run(args.run_id, subprocess_runner(args.gh or ["gh"]),
                                  repo=args.repo, baseline=baseline)
    except (ValueError, GhError) as exc:
        ci_step.emit_error(f"log_budget.py: error: {exc}")
        return EXIT_USAGE
    verdict = (f"log budget: {len(summaries)} jobs, "
               + (f"{len(problems)} over target" if problems else "within target"))
    ci_step.emit(summaries + bounded(problems) + [verdict])
    return EXIT_TARGET if problems else EXIT_OK


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step.emit([f"error: log_budget.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
