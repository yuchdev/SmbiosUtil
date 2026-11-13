"""Neutral CI environment-name table (ADR 0005, "Neutral environment names").

Every ``scripts/ci/`` script reads its environment **only** through
:func:`get`, and only the names in :data:`NAMES`. The CI adapter (the workflow
``env:`` block today, the TeamCity Kotlin DSL after a migration) maps the
platform's own variables onto these names; scripts never read a platform
variable directly (policy rule P11).

This module and ``reporters/`` are the only ``scripts/ci/`` modules allowed to
know a platform name, and this module uses that knowledge only in the
documentation strings of the mapping table below (the ``github`` and
``teamcity`` columns of :class:`EnvName`) and in the TeamCity secure-name
denylist data (:data:`TEAMCITY_SECURE_NAMES`, Story 02.0 Task 04) that
``reporters.child_environment`` consumes. Nothing here reads them.

Run locally, the accessor fills defaults: ``CI_PLATFORM=local``,
``CI_REPORTER=plain``, and a fresh per-process temporary directory as
``CI_TEMP_DIR`` (with ``ci-logs/``, ``ci-results.jsonl`` and ``artifacts/``
under it). A name marked required that is still missing after defaults makes
:func:`get` print one line to stderr and exit 2 (ADR 0005 exit-code table:
"usage or environment error").

Contract source: ``docs/roadmap/0014-ci-rework-from-scratch/plan.md`` (RC2)
and ``docs/adr/0005-unified-build-package-publish-pipeline.md``.
"""

from __future__ import annotations

import os
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, Optional, Tuple


@dataclass(frozen=True)
class EnvName:
    """One row of the neutral-name table.

    Attributes:
        name: The neutral variable name scripts read.
        meaning: What the value is.
        github: Documentation only - where the GitHub Actions adapter takes it from.
        teamcity: Documentation only - where a TeamCity adapter would take it from.
    """

    name: str
    meaning: str
    github: str
    teamcity: str


#: The neutral-name table, in ADR 0005 order. The two platform columns are
#: documentation strings for the adapter author; no code reads them.
NAMES: Dict[str, EnvName] = {
    row.name: row
    for row in (
        EnvName("CI_PLATFORM", "github / teamcity / local",
                "literal in workflow env:", "literal parameter"),
        EnvName("CI_REPORTER", "reporter backend (github / teamcity / plain)",
                "literal github", "literal teamcity"),
        EnvName("CI_RUN_ID", "unique run id",
                "github.run_id", "%teamcity.build.id%"),
        EnvName("CI_RUN_ATTEMPT", "attempt number of a re-run (S3 staging keys, ADR 0005 D9)",
                "github.run_attempt",
                "literal 1 (teamcity.build.id is already unique per attempt)"),
        EnvName("CI_RUN_URL", "link to the run",
                "github.server_url/github.repository/actions/runs/github.run_id",
                "%teamcity.serverUrl%/viewLog.html?buildId=%teamcity.build.id%"),
        EnvName("CI_EVENT", "push / pr / schedule / manual / tag",
                "mapped from github.event_name + github.ref_type",
                "set per trigger in the DSL"),
        EnvName("CI_REF", "full ref",
                "github.ref", "%teamcity.build.vcs.branch.<root>%"),
        EnvName("CI_REF_NAME", "short ref name",
                "github.ref_name", "%teamcity.build.branch%"),
        EnvName("CI_COMMIT", "commit under test",
                "github.sha", "%build.vcs.number%"),
        EnvName("CI_BASE_COMMIT", "diff base for tiering (empty means full)",
                "PR base sha / github.event.before", "last-built revision, or empty"),
        EnvName("CI_JOB", "stage job",
                "job id", "build configuration id"),
        EnvName("CI_LEG", "leg (linux-x64, ...)",
                "matrix value", "DSL parameter"),
        EnvName("CI_WORKSPACE", "checkout root",
                "github.workspace", "%teamcity.build.checkoutDir%"),
        EnvName("CI_TEMP_DIR", "per-run scratch directory",
                "runner.temp (RUNNER_TEMP)", "%teamcity.build.tempDir%"),
        EnvName("CI_ARTIFACT_DIR", "ADR 0005 D4 artifact root",
                "${runner.temp}/artifacts",
                "checkout-relative directory published via artifact paths"),
        EnvName("CI_LOG_DIR", "RC2 full step logs",
                "under CI_TEMP_DIR", "under CI_TEMP_DIR"),
        EnvName("CI_RESULTS_FILE", "RC3 JSONL step records",
                "under CI_TEMP_DIR", "under CI_TEMP_DIR"),
        EnvName("CI_SUMMARY_FILE", "markdown job summary target (may be empty)",
                "GITHUB_STEP_SUMMARY",
                "empty; the reporter publishes ci-summary.md as an artifact"),
        EnvName("CI_OUTPUT_FILE",
                "neutral job-output channel, written only through Reporter.set_output()",
                "GITHUB_OUTPUT, set for the one stage.py process only, never inherited "
                "by its children",
                "empty; the reporter emits a setParameter service message"),
        EnvName("CI_IMAGE", "RC4 image= value",
                "ImageOS + ImageVersion",
                "%teamcity.agent.name% / %teamcity.agent.jvm.os.name%"),
        EnvName("CI_PUBLISH_TOKEN", "publish-only credential (ADR 0005 D6)",
                "GITHUB_TOKEN passed to publish jobs only",
                "password parameter on the publish configuration only"),
    )
}

#: TeamCity environment names that carry, or point at, password-type parameter
#: values (threat model CR-5). ``reporters.child_environment`` never passes
#: them to a child process. ``TEAMCITY_BUILD_PROPERTIES_FILE`` names the file
#: holding every build parameter, including password-type ones. A TeamCity
#: adapter that must export a password-type parameter to the environment uses
#: one of :data:`TEAMCITY_SECURE_PREFIXES` (or maps it onto ``CI_PUBLISH_*``,
#: which is denied as well), so the denylist needs no per-secret edit.
#: Removing a name is a security change: it needs a threat-model note.
TEAMCITY_SECURE_NAMES: Tuple[str, ...] = ("TEAMCITY_BUILD_PROPERTIES_FILE",)

#: Name prefixes the TeamCity adapter reserves for password-type parameters
#: exported to the environment (see :data:`TEAMCITY_SECURE_NAMES`).
TEAMCITY_SECURE_PREFIXES: Tuple[str, ...] = ("TEAMCITY_SECURE_", "BUILD_SECURE_")

#: Exit code for a usage or environment error (ADR 0005).
EXIT_ENV_ERROR = 2

_REPO_ROOT = Path(__file__).resolve().parents[2]
_local_temp_dir: Optional[str] = None


def _default_temp_dir() -> str:
    """Return a fresh temporary directory, created once per process."""
    global _local_temp_dir  # process-wide memo, not shared mutable config
    if _local_temp_dir is None:
        _local_temp_dir = tempfile.mkdtemp(prefix="ci-")
    return _local_temp_dir


_LOCAL_DEFAULTS: Dict[str, Callable[[], str]] = {
    "CI_PLATFORM": lambda: "local",
    "CI_REPORTER": lambda: "plain",
    "CI_RUN_ID": lambda: "local",
    "CI_RUN_ATTEMPT": lambda: "1",
    "CI_WORKSPACE": lambda: str(_REPO_ROOT),
    "CI_TEMP_DIR": _default_temp_dir,
    "CI_LOG_DIR": lambda: str(Path(get("CI_TEMP_DIR")) / "ci-logs"),
    "CI_RESULTS_FILE": lambda: str(Path(get("CI_TEMP_DIR")) / "ci-results.jsonl"),
    "CI_ARTIFACT_DIR": lambda: str(Path(get("CI_TEMP_DIR")) / "artifacts"),
}


def get(name: str, *, required: bool = False) -> str:
    """Return the value of a neutral environment name, applying local defaults.

    Args:
        name: A name from :data:`NAMES`. Any other name is a programming error.
        required: When true and the value is still empty after defaults, print
            one ``error:`` line to stderr and exit 2.

    Returns:
        The variable's value; ``""`` for an unset, optional name with no default.

    Raises:
        KeyError: ``name`` is not in the neutral-name table.
    """
    if name not in NAMES:
        raise KeyError(f"not a neutral CI name: {name}")
    value = os.environ.get(name, "")
    if not value and name in _LOCAL_DEFAULTS:
        value = _LOCAL_DEFAULTS[name]()
    if not value and required:
        sys.stderr.write(f"error: missing required environment variable {name}\n")
        sys.exit(EXIT_ENV_ERROR)
    return value
