#!/usr/bin/env python3
"""Inventory parity gate: every C01-C22 row is kept in the new workflow (milestone 0014, Story 03.0 Task 04).

Reads ``scripts/ci/inventory.json`` (plan.md's Phase-0 inventory, kept equal
to it by ``tests/test_parity_check.py``) and proves each row against the tree:

- the entry workflow (``ci.yml``; ``ci-next.yml`` before the cutover) calls the
  row's stage workflow, whose matrix holds every leg the row needs;
- for each leg, ``stage.py`` is run **in-process, as a dry run**, once per
  stage-command line the stage workflow runs on that leg (its ``if:`` on
  ``matrix.leg`` evaluated), with every side effect replaced by a recorder:
  ``ci_step.run_step``, the stage collaborators (``bootstrap_vcpkg``,
  ``apt_install``, ``cli_format_check``'s children, ``gui_guards``) and the
  output functions. Nothing is executed, nothing is printed, no file outside
  a temporary directory is written;
- a row's **actual gate is measured**, not declared: each check is forced to
  fail in a second dry run. A non-zero stage exit is ``hard``; a swallowed
  failure of a step the runner treats as best-effort (RC5) is
  ``best-effort``; a swallowed failure of any other step is ``ignored``.
  ``info`` rows must show their ``info:`` line, ``infra`` rows (C03) the vcpkg
  bootstrap/cache sub-steps and cache actions, ``budget`` (C11) the job
  ceilings, step budgets and ``ctest --timeout``, ``policy`` rows (C21, C22)
  a firing one-violation fixture, a clean tree for that rule and a hard
  policy step in ``lint``, and the ``dropped`` row (C05) no reference left;
- every ``actions/checkout`` in the checked workflows has
  ``persist-credentials: false`` (P14, Task 04 "Checkout credentials").

Output (RC1 discrepancy form): one line per discrepancy,
``<ID>: expected <gate> -> actual <gate> (<where>)``, at most 20 plus
``(+<k> more)``; a clean result is one line,
``inventory parity: 21 of 21 kept, 1 dropped (C05: dead)``. ``--table``
prints the row-to-job/step mapping instead (for ``status.md``).

What this cannot prove (real runners only, Task 04): that the same commit is
green on old and new workflows with the same ctest totals, and the negative
proofs. Exit codes: 0 clean, 1 discrepancies, 2 unreadable input. Python 3.9+
standard library only.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import argparse  # noqa: E402
import contextlib  # noqa: E402
import io  # noqa: E402
import json  # noqa: E402
import re  # noqa: E402
import tempfile  # noqa: E402
from dataclasses import dataclass, field  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import (Any, Callable, Dict, Final, Iterator, List, Mapping, Optional,  # noqa: E402
                    Sequence, Set, Tuple)

import apt_install  # noqa: E402
import bootstrap_vcpkg  # noqa: E402
import check_versions  # noqa: E402
import check_workflow_policy as policy_gate  # noqa: E402
import ci_step  # noqa: E402
import cli_format_check  # noqa: E402
import ctest_report  # noqa: E402,F401  (imported so its ci_step reference is the patched one)
import gui_guards  # noqa: E402
import lint  # noqa: E402,F401
import print_env  # noqa: E402
import stage  # noqa: E402
import vcpkg_cache_report  # noqa: E402
from reporters import get_reporter  # noqa: E402

REPO_ROOT: Final = Path(_HERE).resolve().parents[1]
INVENTORY_FILE: Final = Path(_HERE) / "inventory.json"
WORKFLOWS_DIR: Final = REPO_ROOT / ".github" / "workflows"
POLICY_FIXTURES: Final = Path(_HERE) / "tests" / "fixtures" / "workflows"

MAX_LINES: Final = 20
LINE_WIDTH: Final = 160
EXIT_OK: Final = 0
EXIT_DISCREPANCY: Final = 1
EXIT_USAGE: Final = 2

#: Gate strength, weakest first. An actual gate at or above the expected one passes.
STRENGTH: Final[Dict[str, int]] = {"missing": 0, "skipped": 1, "ignored": 2, "info": 3,
                                   "best-effort": 4, "hard": 5}
#: Gates that must match exactly (not on the strength scale).
CATEGORY_GATES: Final = ("infra", "policy", "dead")
#: Environment names a check may inspect (sanitizer run-time options, C13).
TRACED_ENV: Final = ("ASAN_OPTIONS", "UBSAN_OPTIONS")

_RUN_LINE = re.compile(r"^python3? scripts/ci/stage\.py ([a-z]+)(?: --only ([a-z-]+))?$")
_LEG_IF = re.compile(r"^matrix\.leg (==|!=) '([a-z0-9-]+)'$")
_JSON_LIST = re.compile(r"\[[^\[\]]*\]")
_SAFE = re.compile(r"[^A-Za-z0-9 ._/(),:;=|<>\[\]+-]")
#: Directories never scanned for the dropped token (build trees, vendored or GPL sources).
_PRUNE = re.compile(r"^(?:\.git|build.*|cmake-build.*|c-versions|vcpkg_installed|"
                    r"node_modules|__pycache__|\.venv.*)$")


class UsageError(Exception):
    """Unreadable or malformed input (exit 2)."""


# ---------------------------------------------------------------------------
# Inputs
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Tree:
    """Where the checked files live (tests point it at modified copies)."""

    repo_root: Path = REPO_ROOT
    workflows_dir: Path = WORKFLOWS_DIR
    entry: Optional[Path] = None
    legs_file: Path = stage.LEGS_FILE
    test_sets_file: Path = ctest_report.TEST_SETS_FILE
    fixtures_dir: Path = POLICY_FIXTURES
    scan_dirs: Tuple[Path, ...] = ()

    def entry_workflow(self) -> Path:
        """Return the entry workflow: ``ci.yml`` (``ci-next.yml`` before the cutover)."""
        if self.entry is not None:
            return self.entry
        main = self.workflows_dir / "ci.yml"
        return main if main.is_file() else self.workflows_dir / "ci-next.yml"


def load_inventory(path: Path = INVENTORY_FILE) -> List[Dict[str, Any]]:
    """Return the inventory rows.

    Raises:
        UsageError: The file is unreadable or a row lacks ``id``/``gate``/``disposition``/``impl``.
    """
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        rows = data["rows"]
    except (OSError, ValueError, KeyError, TypeError):
        raise UsageError(f"cannot read {path.name}") from None
    if not isinstance(rows, list):
        raise UsageError(f"{path.name}: 'rows' must be a list")
    for row in rows:
        if not (isinstance(row, dict) and isinstance(row.get("id"), str)
                and isinstance(row.get("gate"), str) and row.get("disposition") in ("keep", "drop")
                and isinstance(row.get("impl"), dict)):
            raise UsageError(f"{path.name}: every row needs id, gate, disposition, impl")
    return rows


def _load_yaml(path: Path) -> Any:
    try:
        return policy_gate.load_yaml(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, policy_gate.YamlSubsetError):
        raise UsageError(f"cannot read workflow {path.name}") from None


@dataclass
class StageJob:
    """One job of a stage workflow: its matrix legs and stage-command steps."""

    workflow: str
    name: str
    legs: Tuple[str, ...]
    steps: List[Tuple[Optional[str], str, Optional[str]]]  # (if, stage, only)
    timeout: Optional[int]
    steps_without_timeout: int
    actions: Tuple[str, ...]
    checkouts: List[Optional[str]]  # persist-credentials values


def _matrix_legs(job: Mapping[str, Any]) -> Tuple[str, ...]:
    matrix = (job.get("strategy") or {}).get("matrix") if isinstance(job.get("strategy"),
                                                                     dict) else None
    if isinstance(matrix, dict) and "leg" in matrix:
        value = matrix["leg"]
        if isinstance(value, list):
            return tuple(str(v) for v in value)
        legs: List[str] = []
        for chunk in _JSON_LIST.findall(str(value)):
            try:
                legs += [str(v) for v in json.loads(chunk) if str(v) not in legs]
            except ValueError:
                continue
        return tuple(dict.fromkeys(legs))
    env = job.get("env") if isinstance(job.get("env"), dict) else {}
    leg = env.get("CI_LEG") if isinstance(env, dict) else None
    return (str(leg),) if isinstance(leg, str) and "{" not in leg else ()


def _int(value: Any) -> Optional[int]:
    try:
        return int(str(value))
    except (TypeError, ValueError):
        return None


def parse_stage_workflow(path: Path) -> List[StageJob]:
    """Return every job of a reusable stage workflow."""
    root = _load_yaml(path)
    jobs = root.get("jobs") if isinstance(root, dict) else None
    out: List[StageJob] = []
    for name, job in (jobs or {}).items():
        if not isinstance(job, dict):
            continue
        steps: List[Tuple[Optional[str], str, Optional[str]]] = []
        no_timeout = 0
        actions: List[str] = []
        checkouts: List[Optional[str]] = []
        for step in job.get("steps") or []:
            if not isinstance(step, dict):
                continue
            if _int(step.get("timeout-minutes")) is None:
                no_timeout += 1
            uses = step.get("uses")
            if isinstance(uses, str):
                action = uses.split("@", 1)[0]
                actions.append(action)
                if action == "actions/checkout":
                    with_ = step.get("with") if isinstance(step.get("with"), dict) else {}
                    pc = with_.get("persist-credentials")
                    checkouts.append(None if pc is None else str(pc).lower())
            run = step.get("run")
            if isinstance(run, str):
                match = _RUN_LINE.match(run.strip())
                if match:
                    cond = step.get("if")
                    steps.append((str(cond).strip() if cond is not None else None,
                                  match.group(1), match.group(2)))
        out.append(StageJob(path.name, str(name), _matrix_legs(job), steps,
                            _int(job.get("timeout-minutes")), no_timeout, tuple(actions),
                            checkouts))
    return out


@dataclass
class Workflows:
    """The entry workflow's jobs, resolved to their stage workflows."""

    files: List[Path]
    jobs: Dict[str, List[StageJob]]          # entry job name -> stage jobs
    entry_checkouts: List[Optional[str]]


def load_workflows(tree: Tree) -> Workflows:
    """Parse the entry workflow and every stage workflow it calls.

    Raises:
        UsageError: The entry workflow or a called stage workflow is unreadable.
    """
    entry = tree.entry_workflow()
    root = _load_yaml(entry)
    files = [entry]
    jobs: Dict[str, List[StageJob]] = {}
    entry_checkouts: List[Optional[str]] = []
    for name, job in ((root or {}).get("jobs") or {}).items():
        if not isinstance(job, dict):
            continue
        uses = job.get("uses")
        if isinstance(uses, str) and uses.startswith("./.github/workflows/"):
            path = tree.workflows_dir / uses.rsplit("/", 1)[1]
            if path not in files:
                files.append(path)
            jobs[str(name)] = parse_stage_workflow(path)
        else:
            for step in job.get("steps") or []:
                if isinstance(step, dict) and str(step.get("uses", "")).startswith(
                        "actions/checkout@"):
                    with_ = step.get("with") if isinstance(step.get("with"), dict) else {}
                    pc = with_.get("persist-credentials")
                    entry_checkouts.append(None if pc is None else str(pc).lower())
    return Workflows(files, jobs, entry_checkouts)


def step_applies(cond: Optional[str], leg: str) -> bool:
    """Evaluate a stage step's ``if:`` for ``leg`` (only ``matrix.leg ==/!= '<x>'`` is narrowed)."""
    if cond is None:
        return True
    match = _LEG_IF.match(cond)
    if match is None:
        return True
    return (leg == match.group(2)) == (match.group(1) == "==")


def invocations(job: StageJob, leg: str) -> List[Tuple[str, Optional[str]]]:
    """Return the ``(stage, only)`` stage commands ``job`` runs on ``leg``, in order, deduplicated."""
    out: List[Tuple[str, Optional[str]]] = []
    for cond, stage_name, only in job.steps:
        if step_applies(cond, leg) and (stage_name, only) not in out:
            out.append((stage_name, only))
    return out


# ---------------------------------------------------------------------------
# Dry-run tracer
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Event:
    """One observed sub-step of a dry run."""

    kind: str                  # run | call | info | skip
    name: str
    mode: str = "hard"         # hard | best-effort | info
    argv: Tuple[str, ...] = ()
    env: Tuple[Tuple[str, str], ...] = ()


@dataclass
class Tracer:
    """Records events; makes the one injected ``(kind, name)`` fail."""

    inject: Optional[Tuple[str, str]] = None
    events: List[Event] = field(default_factory=list)

    def fails(self, kind: str, name: str) -> bool:
        return self.inject == (kind, name)

    def record(self, event: Event) -> bool:
        """Record ``event``; return whether it must fail."""
        self.events.append(event)
        return self.fails(event.kind, event.name)


def _env_snapshot() -> Tuple[Tuple[str, str], ...]:
    return tuple((k, os.environ[k]) for k in TRACED_ENV if k in os.environ)


@contextlib.contextmanager
def _patched(tracer: Tracer, temp: Path) -> Iterator[None]:
    """Replace every side effect of a stage run with a recorder (restored on exit)."""

    def run_step(label, argv, *, best_effort_reason=None, **_kw):
        mode = "best-effort" if best_effort_reason else "hard"
        failed = tracer.record(Event("run", str(label), mode, tuple(str(a) for a in argv),
                                     _env_snapshot()))
        outcome = ("warn" if mode == "best-effort" else "fail") if failed else "ok"
        return ci_step.StepResult(str(label), mode, outcome, 1 if failed else 0, 0.0, "", None,
                                  None)

    def call(name: str, rc_fail: int = 1) -> Callable[..., int]:
        def fake(*_a, **_kw) -> int:
            return rc_fail if tracer.record(Event("call", name)) else 0
        return fake

    def vcpkg_main(argv: Sequence[str]) -> int:
        sub = next((a for a in argv if a in ("snapshot", "report")), "?")
        tracer.record(Event("call", f"vcpkg_cache_report {sub}"))
        return 0

    def check_format(py, cli, fmt, sudo, *, ctx=None):
        if tracer.record(Event("call", f"cli format {fmt}")):
            return False, -1, "injected failure"
        return True, (-1 if fmt == "text" else 1), ("non-empty" if fmt == "text"
                                                   else "rows" if fmt == "csv" else "structures")

    def noflag(cli, sudo, *, py=None, ctx=None):
        return (False, 1, 2) if tracer.record(Event("call", "cli no-flag compare")) else (True, 1, 1)

    def sysfs_info(*_a, **_kw) -> str:
        tracer.record(Event("info", "sysfs info", "info"))
        return "info: sysfs"

    def find_binary(build_dir, ctx):
        return (1 if tracer.record(Event("call", "gui binary")) else 0), None

    def print_env_main(argv) -> int:
        tracer.record(Event("info", "env header", "info"))
        return 0

    def skip(self, label: str, reason: str) -> int:
        tracer.record(Event("skip", str(label), "info"))
        return 0

    patches = [
        (ci_step, "run_step", run_step),
        (ci_step, "emit", lambda *a, **k: None),
        (ci_step, "_emit", lambda *a, **k: None),
        (ci_step, "append_record", lambda *a, **k: None),
        (ci_step, "main", call("self-test")),
        (print_env, "main", print_env_main),
        (print_env, "probe_cxx", lambda *a, **k: ("gcc", "13.0")),
        (vcpkg_cache_report, "main", vcpkg_main),
        (vcpkg_cache_report, "cache_key", lambda *a, **k: ("key", "prefix")),
        (bootstrap_vcpkg, "main", call("bootstrap_vcpkg")),
        (apt_install, "main", call("apt_install")),
        (check_versions, "run", call("check_versions")),
        (cli_format_check, "find_cli", lambda *_a, **_k: Path("smbios")),
        (cli_format_check, "check_format", check_format),
        (cli_format_check, "noflag_equals_text", noflag),
        (cli_format_check, "sysfs_info", sysfs_info),
        (gui_guards, "check_find_binary", find_binary),
        (gui_guards, "find_binaries", lambda *_a, **_k: []),
        (gui_guards, "check_hook_strings", call("gui hooks")),
        (gui_guards, "check_local_tier_absent", call("gui local")),
        (stage.Context, "skip", skip),
    ]
    saved = [(obj, name, getattr(obj, name)) for obj, name, _ in patches]
    env_keys = ("CI_LOG_DIR", "CI_RESULTS_FILE", "CI_TEMP_DIR", "CI_REPORTER", "CI_ARTIFACT_DIR")
    saved_env = {k: os.environ.get(k) for k in env_keys + TRACED_ENV}
    try:
        for obj, name, value in patches:
            setattr(obj, name, value)
        os.environ.update({"CI_LOG_DIR": str(temp / "logs"),
                           "CI_RESULTS_FILE": str(temp / "results.jsonl"),
                           "CI_TEMP_DIR": str(temp), "CI_REPORTER": "plain",
                           "CI_ARTIFACT_DIR": str(temp / "artifacts")})
        for key in TRACED_ENV:
            os.environ.pop(key, None)
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            yield
    finally:
        for obj, name, value in saved:
            setattr(obj, name, value)
        for key, value in saved_env.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


def _context(leg: stage.Leg, stage_name: str, temp: Path, tree: Tree) -> stage.Context:
    return stage.Context(stage=stage_name, leg=leg, reporter=get_reporter("plain"),
                         workspace=tree.repo_root, temp_dir=temp, log_dir=temp / "logs",
                         results=temp / "results.jsonl", artifact_dir=temp / "artifacts",
                         event="pr", ref="refs/pull/1/merge", ref_name="1/merge",
                         platform="github", run_id="1")


def dry_run(leg: stage.Leg, calls: Sequence[Tuple[str, Optional[str]]], tree: Tree,
            inject: Optional[Tuple[str, str]] = None) -> Tuple[List[int], List[Event]]:
    """Run ``calls`` for ``leg`` in-process with every side effect recorded; return (exits, events)."""
    tracer = Tracer(inject)
    codes: List[int] = []
    with tempfile.TemporaryDirectory(prefix="parity-") as raw:
        temp = Path(raw)
        (temp / "logs").mkdir()
        with _patched(tracer, temp):
            for stage_name, only in calls:
                try:
                    codes.append(stage.run_stage(_context(leg, stage_name, temp, tree), only))
                except (stage.UsageError, ctest_report.UsageError, ci_step.UnsafePathError):
                    codes.append(EXIT_USAGE)
    return codes, tracer.events


# ---------------------------------------------------------------------------
# Row checks
# ---------------------------------------------------------------------------

def _argv_has(argv: Sequence[str], fragment: str) -> bool:
    return any(a == fragment or fragment in a for a in argv)


def matches(event: Event, check: Mapping[str, Any]) -> bool:
    """Whether ``event`` is the sub-step ``check`` describes."""
    if event.kind != check.get("kind") or event.name != check.get("name"):
        return False
    if not all(_argv_has(event.argv, f) for f in check.get("argv_contains", [])):
        return False
    env = dict(event.env)
    return all(want in env.get(key, "") for key, want in check.get("env_contains", {}).items())


@dataclass
class Finding:
    """The measured result of one inventory row."""

    row: str
    expected: str
    actual: str
    where: str = ""
    mapping: List[str] = field(default_factory=list)


def weakest(gates: Sequence[str]) -> str:
    """Return the weakest gate of ``gates`` (``missing`` for none)."""
    return min(gates, key=lambda g: STRENGTH.get(g, 0)) if gates else "missing"


class Checker:
    """Measures every inventory row against one tree."""

    def __init__(self, tree: Tree) -> None:
        self.tree = tree
        self.workflows = load_workflows(tree)
        self.legs = stage.load_legs(tree.legs_file)
        self._cache: Dict[Tuple[str, Optional[Tuple[str, str]]], Tuple[List[int], List[Event]]] = {}

    # -- leg plumbing -------------------------------------------------------
    def leg(self, name: str) -> Optional[stage.Leg]:
        entry = self.legs.get("legs", {}).get(name)  # type: ignore[union-attr]
        return stage._leg(name, entry) if isinstance(entry, dict) else None

    def stage_job(self, job: str, leg: str) -> Optional[StageJob]:
        """The stage-workflow job behind entry job ``job`` whose matrix holds ``leg``."""
        for sj in self.workflows.jobs.get(job, []):
            if leg in sj.legs:
                return sj
        return None

    def run(self, job: str, leg_name: str,
            inject: Optional[Tuple[str, str]] = None) -> Optional[Tuple[List[int], List[Event]]]:
        key = (f"{job}/{leg_name}", inject)
        if key not in self._cache:
            sj = self.stage_job(job, leg_name)
            leg = self.leg(leg_name)
            if sj is None or leg is None or (job != "lint" and leg.job != job):
                return None
            self._cache[key] = dry_run(leg, invocations(sj, leg_name), self.tree, inject)
        return self._cache[key]

    # -- row kinds ----------------------------------------------------------
    def check_row(self, row: Mapping[str, Any]) -> Finding:
        impl = row["impl"]
        kind = impl.get("kind")
        expected = "dropped" if row["disposition"] == "drop" else row["gate"]
        handler = {"gate": self._gate, "info": self._info, "infra": self._infra,
                   "budget": self._budget, "policy": self._policy,
                   "dropped": self._dropped}.get(kind)
        if handler is None:
            return Finding(row["id"], expected, "unknown", f"impl kind {kind}")
        return handler(row["id"], expected, impl)

    def _observe(self, job: str, leg: str, check: Mapping[str, Any]) -> Tuple[str, Optional[Event]]:
        """Return ``("ok"|"missing"|"skipped"|"no job", event)`` for one check on one leg."""
        result = self.run(job, leg)
        if result is None:
            return "no job", None
        _, events = result
        for event in events:
            if matches(event, check):
                return "ok", event
        if any(e.kind == "skip" and e.name == check.get("name") for e in events):
            return "skipped", None
        return "missing", None

    def _gate(self, rid: str, expected: str, impl: Mapping[str, Any]) -> Finding:
        job = impl["job"]
        gates: List[Tuple[str, str]] = []
        mapping: List[str] = []
        for leg in impl.get("legs", []):
            for check in impl.get("checks", []):
                status, event = self._observe(job, leg, check)
                if event is None:
                    gates.append(("missing" if status != "skipped" else "skipped", leg))
                    continue
                codes, _ = self.run(job, leg, (check["kind"], check["name"]))  # type: ignore[misc]
                if any(codes):
                    gate = "hard"
                elif event.mode == "best-effort":
                    gate = "best-effort"
                else:
                    gate = "ignored"
                gates.append((gate, leg))
                mapping.append(f"{check['name']} [{gate}] ({leg})")
        for leg in impl.get("skip_legs", []):
            for check in impl.get("checks", []):
                status, _ = self._observe(job, leg, check)
                if status != "skipped":
                    gates.append(("missing" if status != "ok" else "hard", leg))
                else:
                    mapping.append(f"{check['name']} [skip: plan ruling] ({leg})")
        return self._verdict(rid, expected, gates, [f"{job}: " + m for m in _group(mapping)])

    def _verdict(self, rid: str, expected: str, gates: Sequence[Tuple[str, str]],
                 mapping: List[str]) -> Finding:
        actual = weakest([g for g, _ in gates])
        where = sorted({leg for g, leg in gates if g == actual and actual != expected})
        return Finding(rid, expected, actual, ",".join(where), mapping)

    def _info(self, rid: str, expected: str, impl: Mapping[str, Any]) -> Finding:
        gates: List[Tuple[str, str]] = []
        mapping: List[str] = []
        for leg in impl.get("legs", []):
            for check in impl.get("checks", []):
                status, event = self._observe(impl["job"], leg, check)
                gates.append(("info" if event is not None else
                              "skipped" if status == "skipped" else "missing", leg))
                if event is not None:
                    mapping.append(f"{check['name']} [info] ({leg})")
        return self._verdict(rid, expected, gates,
                             [f"{impl['job']}: " + m for m in _group(mapping)])

    def _infra(self, rid: str, expected: str, impl: Mapping[str, Any]) -> Finding:
        problems: List[str] = []
        for leg in impl.get("legs", []):
            leg_obj = self.leg(leg)
            job = leg_obj.job if leg_obj else "build"
            for check in impl.get("checks", []):
                if self._observe(job, leg, check)[1] is None:
                    problems.append(f"{leg} {check['name']}")
        for job in impl.get("jobs", []):
            for sj in self.workflows.jobs.get(job, []) or [None]:  # type: ignore[list-item]
                have = set(sj.actions) if sj else set()
                problems += [f"{job} {a}" for a in impl.get("actions", []) if a not in have]
        actual = "infra" if not problems else "missing"
        return Finding(rid, expected, actual, problems[0] if problems else "",
                       [f"{j}: bootstrap_vcpkg, cache report, cache restore/save"
                        for j in impl.get("jobs", [])])

    def _budget(self, rid: str, expected: str, impl: Mapping[str, Any]) -> Finding:
        ceiling = int(impl.get("job_ceiling_minutes", 30))
        want = str(impl.get("ctest_timeout", "60"))
        problems: List[str] = []
        for job, sjs in self.workflows.jobs.items():
            for sj in sjs:
                if sj.timeout is None or sj.timeout > ceiling:
                    problems.append(f"{sj.workflow}/{sj.name} job timeout")
                if sj.steps_without_timeout:
                    problems.append(f"{sj.workflow}/{sj.name} step timeout")
        try:
            timeout, _ = ctest_report.load_sets(self.tree.test_sets_file)
        except ctest_report.UsageError:
            timeout = ""
        if timeout != want:
            problems.append("test_sets.json timeout")
        for (key, inject), (_, events) in list(self._cache.items()):
            if inject is not None:
                continue
            for event in events:
                if event.kind == "run" and event.name.startswith("tests ") and not any(
                        event.argv[i:i + 2] == ("--timeout", want)
                        for i in range(len(event.argv))):
                    problems.append(f"{key} {event.name} ctest --timeout")
        actual = "hard" if not problems else "missing"
        return Finding(rid, expected, actual, problems[0] if problems else "",
                       [f"every job timeout-minutes <= {ceiling}, every step timeout-minutes, "
                        f"ctest --timeout {want}"])

    def _policy(self, rid: str, expected: str, impl: Mapping[str, Any]) -> Finding:
        problems: List[str] = []
        policy = policy_gate.load_policy()
        current = [v for path in self.workflows.files
                   for v in policy_gate.check_workflow(path, policy=policy)]
        for rule in impl.get("rules", []):
            if any(v.rule == rule for v in current):
                problems.append(f"{rule} violated")
        for fixture in impl.get("fixtures", []):
            paths = sorted((self.tree.fixtures_dir / fixture).glob("*.yml"))
            found = {v.rule for p in paths for v in policy_gate.check_workflow(p, policy=policy)}
            if not paths or not found & set(impl.get("rules", [])):
                problems.append(f"{fixture} does not fire")
        policy_step = {"kind": "run", "name": "workflow policy"}
        status, event = self._observe("lint", "linux", policy_step)
        if event is None:
            problems.append("lint has no policy step")
        else:
            codes, _ = self.run("lint", "linux", ("run", "workflow policy"))  # type: ignore[misc]
            if not any(codes):
                problems.append("policy step not hard")
        actual = "policy" if not problems else "missing"
        return Finding(rid, expected, actual, problems[0] if problems else "",
                       [f"lint: workflow policy {','.join(impl.get('rules', []))} [hard]"])

    def _dropped(self, rid: str, expected: str, impl: Mapping[str, Any]) -> Finding:
        token = str(impl["token"])
        hits: List[str] = []
        for path in self.workflows.files:
            if token in path.read_text(encoding="utf-8", errors="replace"):
                hits.append(path.name)
        for base in self.tree.scan_dirs or (Path(_HERE),):
            for path in sorted(base.glob("*")):
                if path.suffix in (".py", ".json") and path.name != INVENTORY_FILE.name and \
                        token in path.read_text(encoding="utf-8", errors="replace"):
                    hits.append(path.name)
        for path in _cmake_files(self.tree.repo_root):
            if token in path.read_text(encoding="utf-8", errors="replace"):
                hits.append(path.name)
        actual = "dropped" if not hits else "referenced"
        return Finding(rid, expected, actual, hits[0] if hits else "",
                       [f"no {token} in the new workflows, scripts/ci or CMake"])

    # -- P14 ----------------------------------------------------------------
    def checkout_credentials(self) -> List[str]:
        """Return one discrepancy line per checkout without ``persist-credentials: false``."""
        out: List[str] = []
        values = [("entry", v) for v in self.workflows.entry_checkouts]
        for sjs in self.workflows.jobs.values():
            values += [(f"{sj.workflow}/{sj.name}", v) for sj in sjs for v in sj.checkouts]
        for where, value in values:
            if value != "false":
                out.append(f"P14 {where}: expected persist-credentials false -> "
                           f"actual {value or 'unset'}")
        return out


def _group(entries: Sequence[str]) -> List[str]:
    """Merge ``<what> (<leg>)`` entries that differ only in the leg: ``<what> (<leg>,<leg>)``."""
    legs: Dict[str, List[str]] = {}
    for entry in entries:
        what, _, leg = entry.rpartition(" (")
        legs.setdefault(what, []).append(leg.rstrip(")"))
    return [f"{what} ({','.join(ls)})" for what, ls in legs.items()]


def _cmake_files(root: Path) -> Iterator[Path]:
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if not _PRUNE.match(d))
        for name in filenames:
            if name == "CMakeLists.txt" or name.endswith(".cmake"):
                yield Path(dirpath) / name


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------

def is_discrepancy(f: Finding) -> bool:
    """Whether a finding breaks parity (weaker gate, wrong category, or not dropped)."""
    if f.expected in CATEGORY_GATES or f.expected in ("dropped",) or \
            f.actual not in STRENGTH or f.expected not in STRENGTH:
        return f.actual != f.expected
    return STRENGTH[f.actual] < STRENGTH[f.expected]


def _clean(text: str) -> str:
    return _SAFE.sub("?", text)


def render(findings: Sequence[Finding], rows: Sequence[Mapping[str, Any]],
           extra: Sequence[str] = ()) -> Tuple[List[str], bool]:
    """Return the report lines and whether parity holds."""
    bad = [f for f in findings if is_discrepancy(f)]
    lines = [_clean(f"{f.row}: expected {f.expected} -> actual {f.actual}"
                    + (f" ({f.where})" if f.where else ""))[:LINE_WIDTH] for f in bad]
    lines += [_clean(x)[:LINE_WIDTH] for x in extra]
    if lines:
        more = len(lines) - MAX_LINES
        return lines[:MAX_LINES] + ([f"(+{more} more)"] if more > 0 else []), False
    kept = [r for r in rows if r["disposition"] == "keep"]
    dropped = [r for r in rows if r["disposition"] == "drop"]
    why = ", ".join(f"{r['id']}: {r['gate']}" for r in dropped)
    return [f"inventory parity: {len(kept)} of {len(kept)} kept, {len(dropped)} dropped"
            + (f" ({why})" if why else "")], True


def table(findings: Sequence[Finding]) -> List[str]:
    """Return the row-to-job/step mapping, one line per row (for status.md)."""
    out = []
    for f in findings:
        where = "; ".join(f.mapping) if f.mapping else f.where
        out.append(_clean(f"{f.row} expected {f.expected}, actual {f.actual}: {where}")[:400])
    return out


def check(tree: Tree = Tree(), inventory: Path = INVENTORY_FILE
          ) -> Tuple[List[Finding], List[str], List[Dict[str, Any]]]:
    """Measure every row; return ``(findings, P14 discrepancies, rows)``.

    Raises:
        UsageError: An input is unreadable.
    """
    rows = load_inventory(inventory)
    try:
        checker = Checker(tree)
    except stage.UsageError as exc:
        raise UsageError(str(exc)) from None
    findings = [checker.check_row(row) for row in rows]
    return findings, checker.checkout_credentials(), rows


def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="parity_check.py", description="Inventory parity gate.")
    p.add_argument("--inventory", default=str(INVENTORY_FILE), help="inventory data file")
    p.add_argument("--workflows-dir", default=str(WORKFLOWS_DIR), help="workflow directory")
    p.add_argument("--entry", default=None, help="entry workflow (default ci.yml, else ci-next.yml)")
    p.add_argument("--table", action="store_true", help="print the row-to-step mapping")
    return p


def main(argv: Sequence[str]) -> int:
    """Run the gate; print the report; return 0, 1 or 2."""
    args = _parser().parse_args(list(argv))
    wf_dir = Path(args.workflows_dir)
    tree = Tree(workflows_dir=wf_dir, entry=Path(args.entry) if args.entry else None)
    try:
        findings, extra, rows = check(tree, Path(args.inventory))
    except UsageError as exc:
        ci_step.emit_error(f"parity_check.py: error: {exc}")
        return EXIT_USAGE
    if args.table:
        ci_step.emit(table(findings) + [_clean(x) for x in extra])
    lines, ok = render(findings, rows, extra)
    ci_step.emit(lines)
    return EXIT_OK if ok else EXIT_DISCREPANCY


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step.emit([f"error: parity_check.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
