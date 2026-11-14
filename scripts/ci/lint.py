#!/usr/bin/env python3
"""The one local and CI lint entry point (milestone 0014, Story 02.0 Task 03).

Runs, each through the step runner (``ci_step.run_step``), so success is three
RC1 lines::

    ok: scripts/ci tests: <n> tests (<t>s)
    ok: actionlint: 1.7.12, <k> workflows, shellcheck <yes|no> (<t>s)
    ok: workflow policy (<t>s)

1. ``run_tests.py`` (every ``scripts/ci`` test);
2. actionlint (``install_actionlint.py --run``; the first run downloads the
   pinned release asset, later runs reuse the hash-checked cached archive);
3. ``check_workflow_policy.py`` (P1-P15).

Until the Story 03.0 cutover the workflows under test are
``.github/workflows/ci.yml`` and the reusable ``stage-*.yml`` files it
calls (Story 03.0 Task 01); while none exists, the actionlint step prints one
``skip:`` line and the policy gate still checks the data files, the workflow
directory (P9) and ``scripts/ci`` (P11). Exit: 0 when every step
passes, otherwise the first failing step's exit code. Python 3.9+ stdlib only.
"""

from __future__ import annotations

import shutil
import sys
from pathlib import Path
from typing import List, Sequence

_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

import ci_step  # noqa: E402
import env_names  # noqa: E402
import install_actionlint  # noqa: E402

REPO_ROOT = _HERE.parents[1]
WORKFLOWS_DIR = REPO_ROOT / ".github" / "workflows"
#: Workflows under test: the entry workflow, then the stage workflows in name
#: order. After cutover the entry workflow is ci.yml.
ENTRY_WORKFLOW = WORKFLOWS_DIR / "ci.yml"


def workflows_under_test() -> List[Path]:
    """Return the existing entry workflow and every ``stage-*.yml``, in a stable order."""
    found = [ENTRY_WORKFLOW] if ENTRY_WORKFLOW.is_file() else []
    return found + sorted(p for p in WORKFLOWS_DIR.glob("stage-*.yml") if p.is_file())
ACTIONLINT_TIMEOUT_S = 180.0
POLICY_TIMEOUT_S = 60.0


def main(argv: Sequence[str]) -> int:
    """Run the three lint steps; return the first non-zero exit code, else 0."""
    try:
        reporter = ci_step.get_reporter(None)
    except ValueError as exc:
        ci_step._emit([f"error: lint.py: {exc}"])
        return env_names.EXIT_ENV_ERROR
    log_dir = Path(env_names.get("CI_LOG_DIR", required=True))
    results = Path(env_names.get("CI_RESULTS_FILE", required=True))
    py = [sys.executable, "-I"]
    workflows: List[str] = [str(p) for p in workflows_under_test()]
    exits: List[int] = []

    if "--skip-tests" in argv:
        # Quick pre-push mode: the full suite is ~2 minutes (180+ tests), so
        # the fast loop is actionlint + policy only. CI's lint job omits this flag.
        ci_step._emit(reporter.step_skip("scripts/ci tests", "--skip-tests (fast mode)"))
    else:
        exits.append(ci_step.run_step(
            "scripts/ci tests", py + [str(_HERE / "run_tests.py")], patterns=["python"],
            best_effort_reason=None, summary=None, log_dir=log_dir, reporter=reporter,
            results_file=results).exit)

    if workflows:
        shellcheck = "yes" if shutil.which("shellcheck") else "no"
        exits.append(ci_step.run_step(
            "actionlint", py + [str(_HERE / "install_actionlint.py"), "--run", "--", *workflows],
            patterns=["actionlint"], best_effort_reason=None,
            summary=f"{install_actionlint.VERSION}, {len(workflows)} workflows, "
                    f"shellcheck {shellcheck}",
            log_dir=log_dir, reporter=reporter, results_file=results,
            timeout_s=ACTIONLINT_TIMEOUT_S).exit)
    else:
        ci_step._emit(reporter.step_skip("actionlint", "no workflow under test yet (ci.yml)"))

    exits.append(ci_step.run_step(
        "workflow policy", py + [str(_HERE / "check_workflow_policy.py"), "--reporter", "plain",
                                 *workflows],
        patterns=["policy"], best_effort_reason=None, summary=None, log_dir=log_dir,
        reporter=reporter, results_file=results, timeout_s=POLICY_TIMEOUT_S).exit)
    return next((rc for rc in exits if rc != 0), 0)


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step._emit([f"error: lint.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
