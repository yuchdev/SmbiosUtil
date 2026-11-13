#!/usr/bin/env python3
"""Platform-neutral stage command (milestone 0014 plan SC1; ADR 0005 D8, "Platform portability").

One workflow step is one invocation of this command::

    python3 scripts/ci/stage.py <lint|build|test|package|verify|publish> [--leg <leg>] [--only <step>]

Every sub-step goes through the ``ci_step`` library (RC1-RC3), so a green
sub-step prints exactly one ``ok:``/``info:``/``skip:`` line (plus the RC4
header). This task (Story 03.0 Task 01) implements ``lint``, ``build`` and
``test``; ``package``/``verify``/``publish`` are milestone 0012's and exit 2
until they are defined.

Stages and their steps (``--only`` selects one; without it every step runs in
this order):

``lint`` (Linux only; stops nothing early, every check reports):
    ``env`` (RC4 header), ``self-test`` (``ci_step self-test``), ``lint``
    (``lint.py``: script tests, actionlint, policy gate), ``versions``
    (``check_versions.py``, SC3; ``--tag $CI_REF_NAME`` on a tag event),
    ``cversions`` (inventory C01), ``doc-samples`` (C02).
``build`` (stops at the first failing step):
    ``prepare`` - pinned tools into a job-local venv (``$CI_TEMP_DIR/ci-tools``,
    printed after the header), the RC4 header, ``ci_step self-test``, and the
    vcpkg cache key, exported as the job outputs ``cache`` (``{"key",
    "restore_prefix"}``) and ``cache_enabled`` (``false`` on a tag-event
    packaging leg, CR-2);
    ``compile`` - ``bootstrap_vcpkg.py`` (in-process, so ``VCPKG_ROOT`` and
    ``VCPKG_BINARY_SOURCES`` reach configure), configure (C04), the cache-health
    line (C03), build (C04). A configure/build log above 50 MiB is renamed so
    the workflow's literal-path log upload skips it, with one ``info:`` line
    (plan Q3, threat model CR-10);
    ``install`` - packaging legs only (SC2): ``cmake --install`` into
    ``$CI_ARTIFACT_DIR/install/<leg>/`` and the tree digest exported as the job
    output ``install_digest`` (CR-8). Packaging legs stay ``pending`` in
    ``legs.json`` until milestone 0012 names the shipped configuration and adds
    the ``libsmbios`` install rules, so today this step prints ``skip:``.
``test`` (every step runs; the stage fails if any did):
    ``unit`` (hard; C06 on Windows, C13 on the sanitizer legs; legs with
    ``unit_tests``), ``cli-formats`` (C07, C08, C09; ``cli_format_check.py``;
    not on sanitizer legs), ``live-table`` (best-effort, counts-only; C10 on
    Windows, plan Q4 under ``sudo -n`` on Linux, C14 on ``macos-asan``; legs
    with ``live_table_tests``, else one ``skip:`` line). ``unit`` and
    ``live-table`` run the ``test_sets.json`` sets through ``ctest_report.py``
    (Story 03.0 Task 02): the exit code decides, the counts come from
    ``ctest --output-junit``.

Sanitizer legs (Story 03.0 Task 02; legs with ``"job": "sanitizers"``,
inventory C12-C14): ``build`` configures into ``build/<preset>-asan`` with
:data:`SANITIZER_CXX_FLAGS`/:data:`SANITIZER_LINK_FLAGS` and uses the ``asan``
vcpkg cache flavor; ``test`` runs with :func:`sanitizer_env` (``ASAN_OPTIONS``
as the old workflow, fatal ``UBSAN_OPTIONS``, plan Q2/D2).

GUI legs (Story 03.0 Task 03; legs with ``"job": "gui"``, inventory C15-C20)
run their own step lists (:func:`steps_for`):
    ``build``: ``packages`` (C15, ``apt_install.py``; legs with
    ``apt_packages``, else ``skip:``; a local run never calls ``sudo apt``),
    then ``prepare`` and ``compile`` as above (``cmake --preset <os>-gui``,
    ``gui`` vcpkg cache flavor; C16).
    ``test`` (every step runs): ``gui-binary`` (C17), ``gui-hooks`` (R-7, C18),
    ``gui-ci`` (C19, the ``gui_ci`` set through ``ctest_report.py``) and
    ``gui-local`` (R-8, C20); C17, C18 and C20 are ``gui_guards.py``. No step
    executes ``smbios_gui``.
A step named with ``--only`` that is not in the leg's own list prints one
``skip:`` line.

Input validation (threat model CR-6): ``--leg`` (else ``$CI_LEG``), ``--only``,
``$CI_EVENT`` and ``$CI_REF`` are re-validated against this script's own
allowlists (``legs.json``, the stage's step names, the neutral event names);
an unknown value exits 2 with a fixed message that never echoes it. The
platform's own validation is never relied on.

Trigger tiering (plan Q1): a leg not selected for ``$CI_EVENT``/``$CI_REF``
prints one ``skip:`` line and exits 0 (the workflow does not start such legs;
this is the portable self-skip of ADR 0005 "Trigger tiering").

Exit codes (SC1): 0 success, 1 a gate failed, 2 usage or environment error,
128+n a sub-step killed by signal n. Run locally with the defaults of
``env_names`` (``CI_REPORTER=plain``, temporary directories); the leg then
defaults to the host OS. Python 3.9+ standard library only.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import argparse  # noqa: E402
import contextlib  # noqa: E402
import hashlib  # noqa: E402
import json  # noqa: E402
import platform  # noqa: E402
import re  # noqa: E402
import shutil  # noqa: E402
import stat  # noqa: E402
import time  # noqa: E402
from dataclasses import dataclass, field  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import (Callable, Dict, Final, Iterator, List, Mapping, Optional,  # noqa: E402
                    Sequence, Tuple)

import ci_step  # noqa: E402
import env_names  # noqa: E402
from reporters import Reporter, get_reporter  # noqa: E402

REPO_ROOT: Final = Path(_HERE).resolve().parents[1]
LEGS_FILE: Final = Path(_HERE) / "legs.json"
PINS_FILE: Final = Path(_HERE) / "pins.json"
REQUIREMENTS_FILE: Final = Path(_HERE) / "requirements-tools.txt"

#: Every stage name of SC1, in pipeline order.
STAGES: Final = ("lint", "build", "test", "package", "verify", "publish")
#: Steps of each implemented stage, in run order (the ``--only`` allowlist).
STEPS: Final[Dict[str, Tuple[str, ...]]] = {
    "lint": ("env", "self-test", "lint", "versions", "cversions", "doc-samples"),
    "build": ("packages", "prepare", "compile", "install"),
    "test": ("unit", "cli-formats", "live-table", "gui-binary", "gui-hooks", "gui-ci",
             "gui-local"),
}
#: Run order of each (stage, job); a stage absent here runs all of ``STEPS``.
JOB_STEPS: Final[Dict[Tuple[str, str], Tuple[str, ...]]] = {
    ("build", "build"): ("prepare", "compile", "install"),
    ("build", "sanitizers"): ("prepare", "compile", "install"),
    ("build", "gui"): ("packages", "prepare", "compile"),
    ("test", "build"): ("unit", "cli-formats", "live-table"),
    ("test", "sanitizers"): ("unit", "cli-formats", "live-table"),
    ("test", "gui"): ("gui-binary", "gui-hooks", "gui-ci", "gui-local"),
}
#: Neutral ``CI_EVENT`` values (ADR 0005); empty means a local run.
EVENTS: Final = ("push", "pr", "schedule", "manual", "tag")
#: The only branch whose pushes run the full matrix (plan Q1).
MASTER_REF: Final = "refs/heads/master"

#: RC5 reason of the C10/C14 best-effort step (wording kept from the old workflow).
LIVE_TABLE_REASON: Final = "a virtualized runner's firmware may not expose an SMBIOS table"
#: RC5 ``skip:`` reason of the live-table step on the Linux sanitizer leg (plan Q4 ruling).
LIVE_TABLE_ASAN_SKIP: Final = ("Q4: reading the table needs root on Linux; not run under "
                               "sanitizers (build linux runs it under sudo)")
#: Jobs a leg can belong to (the workflow job that runs it).
JOBS: Final = ("build", "sanitizers", "gui")

#: Inventory C12 sanitizer compile flags (the old ``ci.yml``'s), plus
#: ``-fno-sanitize-recover=undefined`` (plan Q2/D2, E11): without it UBSan
#: reports and continues, the test exits 0 and the report is never shown.
SANITIZER_CXX_FLAGS: Final = ("-fsanitize=address,undefined -fno-omit-frame-pointer -g "
                              "-fno-sanitize-recover=undefined")
#: Inventory C12 sanitizer link flags (unchanged from the old ``ci.yml``).
SANITIZER_LINK_FLAGS: Final = "-fsanitize=address,undefined"
#: Fatal UBSan at run time too (plan Q2/D2); the stack trace goes to the log only.
UBSAN_OPTIONS: Final = "halt_on_error=1:print_stacktrace=1"

#: Step labels whose logs the workflow uploads on failure (plan Q3), and the cap.
UPLOADED_LOGS: Final = ("configure", "build")
LOG_UPLOAD_CAP: Final = 50 << 20
#: Job-local venv for the pinned tools, under ``$CI_TEMP_DIR``.
TOOLS_VENV: Final = "ci-tools"
#: Time limits of the network-bound sub-steps (the workflow step budget is the outer bound).
PIP_TIMEOUT_S: Final = 300.0
VENV_TIMEOUT_S: Final = 120.0

_LEG_NAME = re.compile(r"^[a-z0-9][a-z0-9-]{0,31}$")
_REF = re.compile(r"^refs/[A-Za-z0-9._/+-]{1,200}$")
_REF_NAME = re.compile(r"^[A-Za-z0-9._/+-]{1,200}$")
_PATH_TOKEN = re.compile(r"^[A-Za-z0-9._/+-]{1,120}$")
_HOST_LEGS: Final = {"Linux": "linux", "Darwin": "macos", "Windows": "windows"}

EXIT_OK: Final = 0
EXIT_GATE: Final = 1
EXIT_USAGE: Final = env_names.EXIT_ENV_ERROR


class UsageError(Exception):
    """A usage or environment error (exit 2). The message never echoes the bad value."""


class DigestError(Exception):
    """The install tree cannot be digested (symlink, special file, ``Testing/``, empty)."""


# ---------------------------------------------------------------------------
# Legs, tiers and input validation (CR-6)
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Leg:
    """One ``legs.json`` entry."""

    name: str
    runner: str
    preset: str
    config: Optional[str]
    live_table_sudo: bool = False
    sysfs_info: bool = False
    packaging: bool = False
    unit_tests: bool = False
    live_table_tests: bool = False
    pending: Optional[str] = None
    job: str = "build"
    apt_packages: bool = False

    @property
    def sanitizers(self) -> bool:
        """Whether this leg is a sanitizer leg (``build/<preset>-asan``, C12-C14)."""
        return self.job == "sanitizers"

    @property
    def gui(self) -> bool:
        """Whether this leg is a GUI leg (``build/<os>-gui``, C15-C20)."""
        return self.job == "gui"


def load_legs(path: Path = LEGS_FILE) -> Mapping[str, object]:
    """Return the parsed ``legs.json``.

    Raises:
        UsageError: The file is unreadable or not the expected shape.
    """
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        raise UsageError(f"cannot read {path.name}") from None
    if not (isinstance(data, dict) and isinstance(data.get("legs"), dict)
            and isinstance(data.get("stages"), dict) and isinstance(data.get("tiers"), dict)):
        raise UsageError(f"{path.name}: expected 'stages', 'tiers' and 'legs' objects")
    return data


def _leg(name: str, entry: Mapping[str, object]) -> Leg:
    def flag(key: str) -> bool:
        return entry.get(key) is True

    config = entry.get("config")
    pending = entry.get("pending")
    job = entry.get("job", "build")
    if not (isinstance(entry.get("runner"), str) and isinstance(entry.get("preset"), str)
            and (config is None or isinstance(config, str))):
        raise UsageError("legs.json: a leg needs string runner/preset and a string or null config")
    if job not in JOBS:
        raise UsageError(f"legs.json: a leg's job must be one of: {', '.join(JOBS)}")
    return Leg(name, str(entry["runner"]), str(entry["preset"]), config,
               flag("live_table_sudo"), flag("sysfs_info"), flag("packaging"),
               flag("unit_tests"), flag("live_table_tests"),
               pending if isinstance(pending, str) else None, str(job),
               flag("apt_packages"))


def validate_leg(stage: str, value: Optional[str], legs: Mapping[str, object]) -> Leg:
    """Return the :class:`Leg` named ``value`` if ``stage`` declares it.

    Raises:
        UsageError: ``value`` is malformed, unknown, not a leg of ``stage``, or
            a packaging leg still ``pending`` (milestone 0012).
    """
    if value is None or not _LEG_NAME.fullmatch(value):
        raise UsageError("leg must match [a-z0-9][a-z0-9-]* (see scripts/ci/legs.json)")
    entries = legs.get("legs")
    stage_legs = legs.get("stages", {}).get(stage, [])  # type: ignore[union-attr]
    if not isinstance(entries, dict) or value not in entries or value not in stage_legs:
        raise UsageError(f"unknown leg for stage {stage} (not in scripts/ci/legs.json)")
    leg = _leg(value, entries[value])
    if leg.pending:
        raise UsageError(f"leg {value} is pending: {leg.pending}")
    return leg


def validate_only(stage: str, only: Optional[str]) -> Optional[str]:
    """Return ``only`` if it names a step of ``stage``.

    Raises:
        UsageError: Unknown step (the value is not echoed).
    """
    if only is None:
        return None
    if only not in STEPS[stage]:
        raise UsageError(f"--only must be one of: {', '.join(STEPS[stage])}")
    return only


def validate_event(event: str, ref: str, ref_name: str) -> None:
    """Check the neutral ``CI_EVENT``/``CI_REF``/``CI_REF_NAME`` values (empty is a local run).

    Raises:
        UsageError: A value is outside its allowlist.
    """
    if event and event not in EVENTS:
        raise UsageError(f"CI_EVENT must be one of: {', '.join(EVENTS)}")
    if ref and not _REF.fullmatch(ref):
        raise UsageError("CI_REF is not a well-formed ref")
    if ref_name and not _REF_NAME.fullmatch(ref_name):
        raise UsageError("CI_REF_NAME is not a well-formed ref name")


def tier_legs(event: str, ref: str, legs: Mapping[str, object]) -> Optional[Tuple[str, ...]]:
    """Return the legs the Q1 tier runs for this event, or ``None`` for a local run (all legs).

    A ``push`` to a branch other than ``master`` runs ``tiers.push_branch``;
    every other event runs ``tiers.full`` (plan Q1, ADR 0005 D7).
    """
    if not event:
        return None
    tiers = legs.get("tiers", {})
    name = "push_branch" if (event == "push" and ref.startswith("refs/heads/")
                             and ref != MASTER_REF) else "full"
    selected = tiers.get(name, []) if isinstance(tiers, dict) else []
    return tuple(str(x) for x in selected)


def leg_selected(leg: str, event: str, ref: str, legs: Mapping[str, object]) -> bool:
    """Return whether the Q1 tier runs ``leg`` for this event (always true locally)."""
    selected = tier_legs(event, ref, legs)
    return selected is None or leg in selected


def host_leg() -> str:
    """Return the leg matching this host's OS (local default)."""
    return _HOST_LEGS.get(platform.system(), "linux")


# ---------------------------------------------------------------------------
# Install-tree digest (SC2, CR-8, CR-9)
# ---------------------------------------------------------------------------

def _show(rel: str) -> str:
    return rel if _PATH_TOKEN.match(rel) else "<path>"


def tree_digest(root: Path) -> Tuple[str, int]:
    """Return ``(sha256 hex, file count)`` of the installed tree under ``root``.

    The digest is SHA-256 over the sorted relative POSIX paths, each followed
    by its file's SHA-256 (``<path>\\0<sha256>\\n`` per file). Directories add
    nothing on their own.

    Raises:
        DigestError: ``root`` is missing, a symlink or not a directory; the tree
            holds a symlink, a non-regular file, or a path component named
            ``Testing`` (the build tree's CTest directory must never ship,
            CR-9); or the tree has no file.
    """
    try:
        st = os.lstat(root)
    except OSError:
        raise DigestError("install tree missing") from None
    if stat.S_ISLNK(st.st_mode) or not stat.S_ISDIR(st.st_mode):
        raise DigestError("install tree root is not a directory")
    files: List[Tuple[str, Path]] = []
    for dirpath, dirnames, filenames in os.walk(root, followlinks=False):
        here = Path(dirpath)
        for name in sorted(dirnames) + sorted(filenames):
            path = here / name
            rel = path.relative_to(root).as_posix()
            if name == "Testing":
                raise DigestError(f"install tree contains a Testing path: {_show(rel)}")
            mode = os.lstat(path).st_mode
            if stat.S_ISLNK(mode):
                raise DigestError(f"install tree contains a symlink: {_show(rel)}")
            if name in filenames:
                if not stat.S_ISREG(mode):
                    raise DigestError(f"install tree contains a non-regular file: {_show(rel)}")
                files.append((rel, path))
    if not files:
        raise DigestError("install tree is empty")
    digest = hashlib.sha256()
    for rel, path in sorted(files):
        file_hash = hashlib.sha256()
        with open(path, "rb") as fh:
            for chunk in iter(lambda: fh.read(1 << 20), b""):
                file_hash.update(chunk)
        digest.update(rel.encode("utf-8") + b"\0" + file_hash.hexdigest().encode("ascii") + b"\n")
    return digest.hexdigest(), len(files)


# ---------------------------------------------------------------------------
# Stage context and step helpers
# ---------------------------------------------------------------------------

class _QuietOk:
    """Reporter proxy that suppresses ``ok:`` lines; failures render normally."""

    def __init__(self, inner: Reporter) -> None:
        self._inner = inner
        self.name = inner.name

    def step_ok(self, label: str, summary: str, elapsed_s: float) -> List[str]:  # noqa: D102
        return []

    def __getattr__(self, attr: str):
        return getattr(self._inner, attr)


@dataclass
class Context:
    """Everything a step needs; built once per invocation from the neutral names."""

    stage: str
    leg: Leg
    reporter: Reporter
    workspace: Path
    temp_dir: Path
    log_dir: Path
    results: Path
    artifact_dir: Path
    event: str = ""
    ref: str = ""
    ref_name: str = ""
    platform: str = "local"
    run_id: str = "local"
    py: str = field(default_factory=lambda: sys.executable)
    pins_file: Path = PINS_FILE

    @property
    def local(self) -> bool:
        """Whether this is a developer-machine run (``CI_PLATFORM=local``)."""
        return self.platform == "local"

    @property
    def build_dir(self) -> Path:
        """The preset's build tree, ``build/<preset>`` (``build/<preset>-asan`` on sanitizer legs)."""
        suffix = "-asan" if self.leg.sanitizers else ""
        return self.workspace / "build" / (self.leg.preset + suffix)

    def run(self, label: str, argv: Sequence[str], *, patterns: Sequence[str],
            best_effort: Optional[str] = None, summary: Optional[str] = None,
            hygiene: ci_step.Hygiene = "full", timeout_s: Optional[float] = None,
            reporter: Optional[Reporter] = None) -> int:
        """Run one sub-step through ``ci_step.run_step``; return its exit (0 for best-effort)."""
        result = ci_step.run_step(label, argv, patterns=patterns, best_effort_reason=best_effort,
                                  summary=summary, log_dir=self.log_dir,
                                  reporter=reporter or self.reporter, results_file=self.results,
                                  timeout_s=timeout_s, hygiene=hygiene)
        return 0 if result.mode == "best-effort" else result.exit

    def emit(self, lines: List[str]) -> None:
        """Print reporter-rendered lines (the single emitter, P11)."""
        ci_step.emit(lines)

    def skip(self, label: str, reason: str) -> int:
        """Print one RC5 ``skip:`` line and record it; return 0."""
        self.emit(self.reporter.step_skip(label, reason))
        ci_step.append_record(ci_step.StepResult(label, "hard", "skip", 0, 0.0, reason, None,
                                                 None), self.results)
        return 0

    def fail(self, label: str, first: str, start: float, excerpt: Sequence[str] = ()) -> int:
        """Print one RC1 failure for a stage-level check and record it; return 1."""
        elapsed = round(time.monotonic() - start, 1)
        self.emit(self.reporter.step_fail(label, 1, elapsed, first, list(excerpt)))
        ci_step.append_record(ci_step.StepResult(label, "hard", "fail", 1, elapsed, "", first,
                                                 None), self.results)
        return EXIT_GATE


def _script(ctx: Context, rel: str, *args: str) -> List[str]:
    return [ctx.py, "-I", str(ctx.workspace / rel), *args]


def _venv_bin(temp_dir: Path) -> Path:
    return temp_dir / TOOLS_VENV / ("Scripts" if os.name == "nt" else "bin")


def activate_tools(temp_dir: Path) -> bool:
    """Put the job-local tools venv first on ``PATH`` if a previous step created it.

    Only a real directory is used (never a symlink), so a planted path cannot
    redirect ``cmake``. Returns whether the venv was activated.
    """
    venv = temp_dir / TOOLS_VENV
    bindir = _venv_bin(temp_dir)
    try:
        if venv.is_symlink() or bindir.is_symlink() or not bindir.is_dir():
            return False
    except OSError:
        return False
    path = os.environ.get("PATH", "")
    if not path.startswith(str(bindir) + os.pathsep):
        os.environ["PATH"] = str(bindir) + os.pathsep + path
    return True


# ---------------------------------------------------------------------------
# Common steps
# ---------------------------------------------------------------------------

def step_env(ctx: Context) -> int:
    """RC4 header (``print_env.py``, in-process so the summary goes to this job)."""
    import print_env
    print_env.main(["--job", ctx.stage, "--leg", ctx.leg.name, "--reporter", ctx.reporter.name])
    return 0


def step_self_test(ctx: Context) -> int:
    """``ci_step self-test`` (one ``ok:`` line)."""
    return ci_step.main(["self-test", "--reporter", ctx.reporter.name])


# ---------------------------------------------------------------------------
# lint
# ---------------------------------------------------------------------------

def step_lint(ctx: Context) -> int:
    """``lint.py``: script tests, actionlint and the policy gate (three ``ok:`` lines)."""
    import lint
    return lint.main([])


def step_versions(ctx: Context) -> int:
    """SC3 version consistency; on a tag event the tag name is checked too."""
    import check_versions
    tag = ctx.ref_name if ctx.event == "tag" else None
    return check_versions.run(ctx.workspace, tag=tag, reporter=ctx.reporter, results=ctx.results)


def step_cversions(ctx: Context) -> int:
    """Inventory C01: nothing builds, includes or tracks the GPL ``c-versions/``."""
    return ctx.run("c-versions isolation", _script(ctx, "scripts/check_cversions_isolation.py"),
                   patterns=["python"])


def step_doc_samples(ctx: Context) -> int:
    """Inventory C02: doc output samples equal the golden files."""
    return ctx.run("doc samples", _script(ctx, "scripts/check_doc_samples.py", "--check"),
                   patterns=["python"])


# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------

def _runner_label(ctx: Context) -> str:
    try:
        pins = json.loads(ctx.pins_file.read_text(encoding="utf-8"))
        label = pins["runners"][ctx.leg.runner]
    except (OSError, ValueError, KeyError, TypeError):
        raise UsageError("pins.json has no runner label for this leg") from None
    if not isinstance(label, str) or not re.fullmatch(r"[0-9A-Za-z._-]{1,40}", label):
        raise UsageError("pins.json runner label is malformed")
    return label


def cold_build(ctx: Context) -> bool:
    """Return whether this leg builds without any binary cache (tag-event packaging leg, CR-2)."""
    return ctx.event == "tag" and ctx.leg.packaging


def _tools(ctx: Context) -> Tuple[int, Optional[Tuple[str, float]]]:
    """Install the pinned tools into the job-local venv; return ``(exit, deferred ok line)``.

    The ``ok:`` line is printed later, after the RC4 header (which then shows
    the pinned versions). A local run uses the host tools and skips.
    """
    if ctx.local:
        return 0, None
    start = time.monotonic()
    quiet = _QuietOk(ctx.reporter)
    venv = ctx.temp_dir / TOOLS_VENV
    if venv.is_symlink():
        return ctx.fail("pinned tools", "tools venv path is a symlink", start), None
    rc = ctx.run("tools venv", [ctx.py, "-I", "-m", "venv", str(venv)], patterns=["python"],
                 timeout_s=VENV_TIMEOUT_S, reporter=quiet)
    if rc != 0:
        return rc, None
    venv_py = _venv_bin(ctx.temp_dir) / ("python.exe" if os.name == "nt" else "python")
    rc = ctx.run("pinned tools", [str(venv_py), "-I", "-m", "pip", "install", "--quiet",
                                  "--disable-pip-version-check", "--require-hashes", "--no-deps",
                                  "--only-binary", ":all:", "-r", str(REQUIREMENTS_FILE)],
                 patterns=["python"], timeout_s=PIP_TIMEOUT_S, reporter=quiet)
    if rc != 0:
        return rc, None
    activate_tools(ctx.temp_dir)
    return 0, ("cmake, ninja from requirements-tools.txt", round(time.monotonic() - start, 1))


def _set_cache_output(ctx: Context, key: Optional[Tuple[str, str]]) -> None:
    """Export ``cache`` (``{"key", "restore_prefix"}``) and ``cache_enabled`` (a JSON boolean).

    ``cache_enabled`` is separate so the workflow's guards stay plain
    comparisons the policy gate can evaluate (P15).
    """
    ctx.reporter.set_output("cache", {"key": key[0] if key else "",
                                      "restore_prefix": key[1] if key else ""})
    ctx.reporter.set_output("cache_enabled", key is not None)


def step_cache_key(ctx: Context) -> int:
    """Export the vcpkg cache key as the job output ``cache`` (Story 02.0 Task 02 scheme)."""
    import print_env
    import vcpkg_cache_report
    if cold_build(ctx):
        _set_cache_output(ctx, None)
        return ctx.skip("vcpkg cache", "tag-event packaging leg builds cold (CR-2)")
    cxx = vcpkg_cache_report.cxx_tag(*print_env.probe_cxx())
    run_id = ctx.run_id if re.fullmatch(r"[0-9A-Za-z._-]{1,40}", ctx.run_id) else "local"
    try:
        flavor = "asan" if ctx.leg.sanitizers else "gui" if ctx.leg.gui else "default"
        key, prefix = vcpkg_cache_report.cache_key(flavor, _runner_label(ctx), cxx,
                                                   ctx.workspace, run_id)
    except (OSError, ValueError) as exc:
        return ctx.fail("vcpkg cache key", f"cannot compute the key ({type(exc).__name__})",
                        time.monotonic())
    _set_cache_output(ctx, (key, prefix))
    ctx.emit(ctx.reporter.info(f"info: vcpkg cache key: {key}"))
    return 0


def step_packages(ctx: Context) -> int:
    """C15: the GUI leg's apt packages (``apt_install.py``, in-process); ``skip:`` elsewhere."""
    if not ctx.leg.apt_packages:
        return ctx.skip("apt packages", "no system packages on this leg (C15)")
    if ctx.local:
        return ctx.skip("apt packages", "local run: install scripts/ci/apt-packages-gui.txt "
                                        "yourself (no sudo apt from stage.py)")
    import apt_install
    return apt_install.main(["--reporter", ctx.reporter.name])


def step_prepare(ctx: Context) -> int:
    """Pinned tools, RC4 header, self-test, cache key (one workflow step)."""
    rc, tools_ok = _tools(ctx)
    if rc != 0:
        return rc
    step_env(ctx)
    rc = step_self_test(ctx)
    if rc != 0:
        return rc
    if tools_ok is None:
        ctx.skip("pinned tools", "local run uses the host cmake/ninja")
    else:
        summary, elapsed = tools_ok
        ctx.emit(ctx.reporter.step_ok("pinned tools", summary, elapsed))
    return step_cache_key(ctx)


def cap_upload_logs(ctx: Context) -> None:
    """Keep a configure/build log above 50 MiB out of the failure upload (Q3, CR-10).

    The workflow uploads literal paths only; an oversized log is renamed so
    that path no longer exists, and one ``info:`` line says so.
    """
    for label in UPLOADED_LOGS:
        path = ci_step.log_path_for(label, ctx.log_dir)
        try:
            if path.is_symlink() or not path.is_file():
                continue
            size = path.stat().st_size
            if size > LOG_UPLOAD_CAP:
                path.replace(path.with_name(path.name + ".over-cap"))
                ctx.emit(ctx.reporter.info(
                    f"info: {path.name} is {size >> 20} MiB, above the "
                    f"{LOG_UPLOAD_CAP >> 20} MiB upload cap; not uploaded"))
        except OSError:
            continue


def configure_argv(ctx: Context) -> List[str]:
    """Return the configure argv: the preset, or on a sanitizer leg the C12 configure.

    A sanitizer leg keeps its own binary directory (``-B build/<preset>-asan``)
    so it never clobbers the plain build tree of the same checkout.
    """
    if not ctx.leg.sanitizers:
        return ["cmake", "--preset", ctx.leg.preset]
    return ["cmake", "--preset", ctx.leg.preset, "-B", str(ctx.build_dir),
            f"-DCMAKE_CXX_FLAGS={SANITIZER_CXX_FLAGS}",
            f"-DCMAKE_EXE_LINKER_FLAGS={SANITIZER_LINK_FLAGS}"]


def build_argv(ctx: Context) -> List[str]:
    """Return the build argv (the preset's, or the sanitizer tree's)."""
    if not ctx.leg.sanitizers:
        return ["cmake", "--build", "--preset", ctx.leg.preset]
    return ["cmake", "--build", str(ctx.build_dir)]


def step_compile(ctx: Context) -> int:
    """vcpkg bootstrap, configure, cache health, build (inventory C03, C04, C12)."""
    import bootstrap_vcpkg
    import vcpkg_cache_report
    args = ["--reporter", ctx.reporter.name, "--repo-root", str(ctx.workspace)]
    if ctx.leg.packaging:
        args.append("--packaging-leg")
    rc = bootstrap_vcpkg.main(args)
    if rc != 0:
        return rc
    if ctx.leg.packaging:  # recorded for the 0012 manifest (CR-2)
        cache = "none" if os.environ.get("VCPKG_BINARY_SOURCES") == "clear" else "restored"
        ctx.reporter.set_output("binary_cache", cache)
    vcpkg_cache_report.main(["--reporter", ctx.reporter.name, "snapshot"])
    rc = ctx.run("configure", configure_argv(ctx), patterns=["cmake", "vcpkg"])
    if rc != 0:
        cap_upload_logs(ctx)
        return rc
    vcpkg_cache_report.main(["--reporter", ctx.reporter.name, "report", "--log",
                             str(ci_step.log_path_for("configure", ctx.log_dir))])
    rc = ctx.run("build", build_argv(ctx), patterns=["cxx"])
    if rc != 0:
        cap_upload_logs(ctx)
    return rc


def run_install(ctx: Context) -> int:
    """SC2: ``cmake --install`` into ``install/<leg>/``, then the tree digest as a job output."""
    start = time.monotonic()
    prefix = ctx.artifact_dir / "install" / ctx.leg.name
    if prefix.is_symlink():
        return ctx.fail("install tree", "install prefix is a symlink", start)
    if prefix.exists():
        shutil.rmtree(prefix)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    argv = ["cmake", "--install", str(ctx.build_dir), "--prefix", str(prefix)]
    if ctx.leg.config:
        argv += ["--config", ctx.leg.config]
    rc = ctx.run("install", argv, patterns=["cmake"], reporter=_QuietOk(ctx.reporter))
    if rc != 0:
        return rc
    try:
        digest, count = tree_digest(prefix)
    except DigestError as exc:
        return ctx.fail("install tree", str(exc), start)
    ctx.reporter.set_output("install_digest", {"leg": ctx.leg.name, "sha256": digest,
                                               "files": count})
    elapsed = round(time.monotonic() - start, 1)
    summary = f"{count} files, sha256 {digest[:16]}"
    ctx.emit(ctx.reporter.step_ok("install tree", summary, elapsed))
    ci_step.append_record(ci_step.StepResult("install tree", "hard", "ok", 0, elapsed, summary,
                                             None, None), ctx.results)
    return 0


def step_install(ctx: Context) -> int:
    """Packaging legs only; other legs print ``skip:``."""
    if not ctx.leg.packaging:
        return ctx.skip("install tree", "not a packaging leg")
    return run_install(ctx)


# ---------------------------------------------------------------------------
# test
# ---------------------------------------------------------------------------

def sanitizer_env(leg: Leg) -> Dict[str, str]:
    """Return the sanitizer run-time options of a sanitizer leg (empty for any other leg).

    ``ASAN_OPTIONS`` is kept exactly as the old ``ci.yml`` (inventory C13):

    - ``detect_container_overflow=0``: libc++'s ``vector<string>`` container
      annotations produce a false-positive container-overflow on the ordinary
      ``push_back``-triggered reallocation in
      ``AbstractSMBiosEntry::parse_dmi_strings()`` (verified locally on
      AppleClang/libc++ - not a real out-of-bounds access); a known
      libc++/ASan interaction, not an application bug.
    - ``detect_leaks=0`` (macOS only): AddressSanitizer on macOS does not
      support LeakSanitizer (known platform limitation from milestone 0001).

    ``UBSAN_OPTIONS`` makes UBSan fatal at run time (plan Q2/D2), together
    with ``-fno-sanitize-recover=undefined`` at compile time.
    """
    if not leg.sanitizers:
        return {}
    asan = "detect_container_overflow=0"
    if leg.runner == "macos":
        asan = "detect_leaks=0:" + asan
    return {"ASAN_OPTIONS": asan, "UBSAN_OPTIONS": UBSAN_OPTIONS}


@contextlib.contextmanager
def _child_env(extra: Mapping[str, str]) -> Iterator[None]:
    """Set ``extra`` in ``os.environ`` for the duration of a step (``ci_step`` passes it on)."""
    saved = {k: os.environ.get(k) for k in extra}
    os.environ.update(extra)
    try:
        yield
    finally:
        for key, value in saved.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


def _run_set(ctx: Context, set_name: str, label: str, best_effort: Optional[str],
             sudo: bool = False) -> int:
    """Run one ``test_sets.json`` set through ``ctest_report`` (RC1-RC3 via ``ci_step``)."""
    import ctest_report
    try:
        with _child_env(sanitizer_env(ctx.leg)):
            return ctest_report.run_set(set_name, ctx.build_dir, ctx.leg.config or None, label,
                                        best_effort, sudo=sudo, reporter=ctx.reporter,
                                        log_dir=ctx.log_dir, results=ctx.results,
                                        temp_dir=ctx.temp_dir)
    except ctest_report.UsageError as exc:
        raise UsageError(str(exc)) from None


def step_unit(ctx: Context) -> int:
    """Hard: the ``unprivileged`` set (C06 on Windows, C13 on sanitizer legs)."""
    if not ctx.leg.unit_tests:
        return ctx.skip("tests unprivileged", "run by the sanitizers job on this OS (C13)")
    return _run_set(ctx, "unprivileged", "unprivileged", None)


def step_cli_formats(ctx: Context) -> int:
    """Inventory C07, C08, C09 through ``cli_format_check.py`` (counts only)."""
    if ctx.leg.sanitizers:
        return ctx.skip("cli formats", "run by the build job on this OS (C07)")
    import cli_format_check
    sudo = ctx.leg.live_table_sudo and not ctx.local
    return cli_format_check.run(ctx.build_dir, py=ctx.py, sudo=sudo, sysfs=ctx.leg.sysfs_info,
                                ctx=cli_format_check._Ctx(ctx.reporter, ctx.log_dir, ctx.results))


def step_live_table(ctx: Context) -> int:
    """Best-effort, counts-only: the ``live_table`` set (C10, Q4 sudo on Linux, C14)."""
    if not ctx.leg.live_table_tests:
        reason = (LIVE_TABLE_ASAN_SKIP if ctx.leg.sanitizers
                  else "not run on this leg in the build job (C10, Q4)")
        return ctx.skip("tests live-table", reason)
    sudo = ctx.leg.live_table_sudo and not ctx.local
    return _run_set(ctx, "live_table", "live-table", LIVE_TABLE_REASON, sudo=sudo)


def _gui_ctx(ctx: Context):
    import gui_guards
    return gui_guards.Ctx(ctx.reporter, ctx.results)


def step_gui_binary(ctx: Context) -> int:
    """C17: the shipped ``smbios_gui`` exists under ``build/<os>-gui`` (``gui_guards.py``)."""
    import gui_guards
    return gui_guards.check_find_binary(ctx.build_dir, _gui_ctx(ctx))[0]


def step_gui_hooks(ctx: Context) -> int:
    """R-7 (C18): no test-hook string in any shipped ``smbios_gui``; no binary fails closed.

    The paths come from ``gui_guards.find_binaries`` (CR-12); a search error
    fails closed too (review S3-4).
    """
    import gui_guards
    return gui_guards.check_hook_strings_in_tree(ctx.build_dir, _gui_ctx(ctx))


def step_gui_ci(ctx: Context) -> int:
    """C19: the ``gui_ci`` set (``-L gui-ci``, ``--no-tests=error``), counted from JUnit."""
    return _run_set(ctx, "gui_ci", "gui-ci", None)


def step_gui_local(ctx: Context) -> int:
    """R-8 (C20): the local-only GUI tier is absent (``gui_guards.py``, fail closed)."""
    import gui_guards
    return gui_guards.check_local_tier_absent(ctx.build_dir, ctx.leg.config or None,
                                              _gui_ctx(ctx))


StepFn = Callable[[Context], int]
STEP_FUNCTIONS: Final[Dict[str, Dict[str, StepFn]]] = {
    "lint": {"env": step_env, "self-test": step_self_test, "lint": step_lint,
             "versions": step_versions, "cversions": step_cversions,
             "doc-samples": step_doc_samples},
    "build": {"packages": step_packages, "prepare": step_prepare, "compile": step_compile,
              "install": step_install},
    "test": {"unit": step_unit, "cli-formats": step_cli_formats, "live-table": step_live_table,
             "gui-binary": step_gui_binary, "gui-hooks": step_gui_hooks, "gui-ci": step_gui_ci,
             "gui-local": step_gui_local},
}
#: Stages that stop at the first failing step (later steps depend on earlier ones).
STOP_ON_FAILURE: Final = ("build",)


def _combine(codes: Sequence[int]) -> int:
    """Map sub-step exits to the SC1 stage exit: 0, 1, or 128+n for a signal."""
    for rc in codes:
        if 128 < rc < 160:
            return rc
    return EXIT_GATE if any(codes) else EXIT_OK


def steps_for(stage_name: str, leg: Leg) -> Tuple[str, ...]:
    """Return the steps ``stage_name`` runs on ``leg``'s job, in order."""
    return JOB_STEPS.get((stage_name, leg.job), STEPS[stage_name])


def run_stage(ctx: Context, only: Optional[str]) -> int:
    """Run ``only`` or every step of ``ctx.stage`` for the leg's job; return the SC1 exit code."""
    own = steps_for(ctx.stage, ctx.leg)
    if only and only not in own:
        return ctx.skip(f"{ctx.stage} {only}", f"not a step of the {ctx.leg.job} job")
    names = [only] if only else list(own)
    codes: List[int] = []
    for name in names:
        rc = STEP_FUNCTIONS[ctx.stage][name](ctx)
        codes.append(rc)
        if rc != 0 and ctx.stage in STOP_ON_FAILURE:
            break
    return _combine(codes)


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------

def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="stage.py", description="Run one pipeline stage (SC1).")
    p.add_argument("stage", help="|".join(STAGES))
    p.add_argument("--leg", default=None, help="leg from scripts/ci/legs.json (default $CI_LEG)")
    p.add_argument("--only", default=None, help="run one step of the stage")
    p.add_argument("--reporter", default=None, help="github|teamcity|plain (default $CI_REPORTER)")
    return p


def build_context(stage: str, leg_value: Optional[str], reporter: Reporter,
                  legs: Mapping[str, object]) -> Context:
    """Validate the neutral inputs and return the :class:`Context` (CR-6).

    Raises:
        UsageError: Any input is outside its allowlist.
    """
    plat = env_names.get("CI_PLATFORM")
    if leg_value is None:
        leg_value = env_names.get("CI_LEG") or None
    if leg_value is None and plat == "local":
        leg_value = "linux" if stage == "lint" else host_leg()
    leg = validate_leg(stage, leg_value, legs)
    event = env_names.get("CI_EVENT")
    ref = env_names.get("CI_REF")
    ref_name = env_names.get("CI_REF_NAME")
    validate_event(event, ref, ref_name)
    workspace = Path(env_names.get("CI_WORKSPACE", required=True)).resolve()
    temp_dir = Path(env_names.get("CI_TEMP_DIR", required=True))
    return Context(stage=stage, leg=leg, reporter=reporter, workspace=workspace,
                   temp_dir=temp_dir,
                   log_dir=Path(env_names.get("CI_LOG_DIR", required=True)),
                   results=Path(env_names.get("CI_RESULTS_FILE", required=True)),
                   artifact_dir=Path(env_names.get("CI_ARTIFACT_DIR", required=True)),
                   event=event, ref=ref, ref_name=ref_name, platform=plat,
                   run_id=env_names.get("CI_RUN_ID") or "local")


def main(argv: Sequence[str]) -> int:
    """Validate the inputs, run the stage, return the SC1 exit code (never a traceback)."""
    args = _parser().parse_args(list(argv))
    try:
        if args.stage not in STAGES:
            raise UsageError(f"stage must be one of: {', '.join(STAGES)}")
        if args.stage not in STEPS:
            raise UsageError(f"stage {args.stage} is not defined yet (milestone 0012)")
        only = validate_only(args.stage, args.only)
        try:
            reporter = get_reporter(args.reporter)
        except ValueError:
            raise UsageError("reporter must be one of: github, teamcity, plain") from None
        legs = load_legs()
        ctx = build_context(args.stage, args.leg, reporter, legs)
    except UsageError as exc:
        ci_step.emit_error(f"stage.py: error: {exc}")
        return EXIT_USAGE
    if not leg_selected(ctx.leg.name, ctx.event, ctx.ref, legs):
        if not ctx.local:
            # Review S3-5: on CI the workflow matrix scheduled this leg, so a
            # tier that excludes it is a legs.json/matrix mismatch - a whole
            # job (e.g. every GUI check C17-C20) must not turn into a green skip.
            ci_step.emit_error(f"stage.py: error: leg {ctx.leg.name} is scheduled on CI "
                               f"but not in the {ctx.event} tier "
                               "(legs.json tiers vs workflow matrix)")
            return EXIT_USAGE
        if ctx.stage == "build" and only in (None, "prepare"):
            _set_cache_output(ctx, None)
        return ctx.skip(f"{ctx.stage} {ctx.leg.name}", f"not in the {ctx.event} tier (Q1)")
    try:
        os.chdir(ctx.workspace)
    except OSError:
        ci_step.emit_error("stage.py: error: cannot enter CI_WORKSPACE")
        return EXIT_USAGE
    activate_tools(ctx.temp_dir)
    try:
        return run_stage(ctx, only)
    except UsageError as exc:
        ci_step.emit_error(f"stage.py: error: {exc}")
        return EXIT_USAGE
    except ci_step.UnsafePathError as exc:
        ci_step.emit_error(f"stage.py: error: unsafe path ({exc})")
        return EXIT_USAGE


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except KeyboardInterrupt:
        sys.exit(130)
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step.emit([f"error: stage.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
