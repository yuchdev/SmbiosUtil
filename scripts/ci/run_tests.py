#!/usr/bin/env python3
"""Run every ``scripts/ci`` test and print one RC1 line (milestone 0014, Story 01.0 Task 03).

The one command for local use and for CI's ``lint`` job::

    python3 scripts/ci/run_tests.py        # python on Windows

It runs ``python -m unittest discover -s scripts/ci/tests`` through the step
runner (``ci_step.run_step``), so success is one ``ok: scripts/ci tests: <n>
tests (<t>s)`` line and failure is the bounded RC1 excerpt (``python`` pattern
set) with the full unittest log kept under ``$CI_LOG_DIR``. The test count
comes from the unittest loader, never from parsing unittest's summary text
(plan E4). The reporter backend follows ``$CI_REPORTER`` (default ``plain``).

Exit code: unittest's exit code (0 pass, 1 failure), 2 for an environment
error, 70 for a runner bug. Python 3 standard library only.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import unittest  # noqa: E402
from pathlib import Path  # noqa: E402

import ci_step  # noqa: E402
import env_names  # noqa: E402

TESTS_DIR = Path(_HERE) / "tests"
LABEL = "scripts/ci tests"


def count_tests() -> int:
    """Return the number of test cases ``unittest discover`` will run."""
    sys.path.insert(0, str(TESTS_DIR))
    try:
        return unittest.TestLoader().discover(str(TESTS_DIR)).countTestCases()
    finally:
        sys.path.remove(str(TESTS_DIR))


def main() -> int:
    """Run the suite under the report contract; return its exit code."""
    try:
        count = count_tests()
    except Exception as exc:  # noqa: BLE001 - an import error must stay one line
        count = -1
        ci_step.emit_error(f"run_tests.py: cannot load tests ({type(exc).__name__})")
    try:
        reporter = ci_step.get_reporter(None)
    except ValueError as exc:  # unknown $CI_REPORTER: environment error (ADR 0005)
        ci_step.emit_error(f"run_tests.py: error: {exc}")
        return env_names.EXIT_ENV_ERROR
    result = ci_step.run_step(
        LABEL,
        [sys.executable, "-I", "-m", "unittest", "discover", "-s", str(TESTS_DIR)],
        patterns=["python"], best_effort_reason=None,
        summary=f"{count} tests" if count >= 0 else "test count unavailable",
        log_dir=Path(env_names.get("CI_LOG_DIR", required=True)), reporter=reporter,
        results_file=Path(env_names.get("CI_RESULTS_FILE", required=True)))
    return result.exit


if __name__ == "__main__":
    try:
        sys.exit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step.emit([f"error: run_tests.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
