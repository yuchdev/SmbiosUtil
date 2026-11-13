"""Tests for ``log_budget.py`` and ``log_baseline.json`` (milestone 0014, Story 03.0 Task 04).

Classification: unit tests (no network: ``gh`` is a fake runner callable, or a
Python stub on the command line). Fixtures under ``fixtures/log_budget/`` (``.txt``
because the repository ignores ``*.log`` outside ``fixtures/``'s top level):

- ``jobs.json`` - a ``gh run view --json jobs`` answer with a green new-workflow
  job (101), a failing Windows job (102) and an old-workflow job (103);
- ``green.txt`` - a green job log with a step column: runner echo groups, RC
  lines, a best-effort warning with its excerpt, third-party steps;
- ``failing.txt`` - a failing Compile step (RC1 excerpt) that also carries
  the fake serial ``TEST-SERIAL-0000``;
- ``nosplit.txt`` - an older log without a step column.

Scenarios: counts and split (normal), ``split unavailable`` (edge), an
oversized failing step, a stray line in a green step, a green job above its
baseline (targets missed), bounded discrepancy output, a gh failure and a bad
run id (invalid input), and that no log text (the fake serial) is ever printed.

On failure, first check: the ``gh --log`` line shape ``<job>\\t<step>\\t<ts> <text>``
in ``split_line`` and the RC prefixes in ``log_budget.RC_PREFIXES``.
"""

from __future__ import annotations

import contextlib
import io
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Dict, List, Sequence

from ci_test_support import CI_DIR, FIXTURES

import log_budget as lb  # noqa: E402

FIX = FIXTURES / "log_budget"
SERIAL = "TEST-SERIAL-0000"
PATTERNS = [re.compile(f"^{re.escape(n)}$") for n in
            ("Prepare", "Prepare (Windows)", "Compile", "Compile (Windows)", "Test",
             "Test (Windows)", "Lint stage")]
STATUS = CI_DIR.parents[1] / "docs" / "roadmap" / "0014-ci-rework-from-scratch" / "status.md"


def fake_runner(logs: Dict[str, bytes]) -> lb.Runner:
    """Answer ``run view <id> --json jobs`` from jobs.json and ``--log --job <id>`` from ``logs``."""
    def run(args: Sequence[str]) -> bytes:
        if "--json" in args:
            return (FIX / "jobs.json").read_bytes()
        return logs[args[args.index("--job") + 1]]
    return run


def default_logs() -> Dict[str, bytes]:
    return {"101": (FIX / "green.txt").read_bytes(), "102": (FIX / "failing.txt").read_bytes(),
            "103": (FIX / "nosplit.txt").read_bytes()}


def job(job_id: int) -> dict:
    return next(j for j in json.loads((FIX / "jobs.json").read_text())["jobs"]
                if j["databaseId"] == job_id)


class BaselineDataTest(unittest.TestCase):
    """log_baseline.json carries status.md's recorded numbers."""

    def test_baseline_numbers_match_status(self) -> None:
        text = STATUS.read_text(encoding="utf-8")
        base = lb.load_baseline()
        self.assertIn(f"run {base['green_run']}", text)
        for name, row in base["jobs"].items():
            job_kind, os_name = re.match(r"^(\S+) \((\S+?)(?:-latest)?\)$", name).groups()
            os_word = {"ubuntu": "ubuntu", "ubuntu-24.04": "ubuntu-24.04"}.get(os_name, os_name)
            with self.subTest(job=name):
                self.assertIn(f"{job_kind} {os_word} {row['bytes']} / {row['lines']} / "
                              f"{row['duration_s']}s", text)

    def test_every_full_tier_leg_has_a_baseline(self) -> None:
        legs = json.loads((CI_DIR / "legs.json").read_text())["tiers"]["full"]
        base = lb.load_baseline()
        self.assertEqual(sorted(base["leg_map"]), sorted(legs))
        for old in base["leg_map"].values():
            self.assertIn(old, base["jobs"])

    def test_targets_are_the_spec_values(self) -> None:
        targets = lb.load_baseline()["targets"]
        self.assertEqual(targets["failing_step_max_lines"], 64)
        self.assertEqual(targets["failing_step_max_bytes"], 16 * 1024)


class MeasureTest(unittest.TestCase):
    """Counting one job log."""

    def test_green_job_split(self) -> None:
        m = lb.measure(job(101), (FIX / "green.txt").read_bytes(), PATTERNS)
        self.assertTrue(m.split)
        self.assertEqual(m.lines, 44)
        self.assertEqual(m.echo, 18)
        self.assertEqual(m.project, 15)
        self.assertEqual(m.third_party, 11)
        self.assertEqual(m.project + m.echo + m.third_party, m.lines)
        self.assertEqual(m.bytes, len((FIX / "green.txt").read_bytes()))
        self.assertEqual(m.failed_steps, [])
        self.assertTrue(m.steps["Test"].warned)
        self.assertEqual(lb.check_targets(m, lb.load_baseline()), [])

    def test_failing_step_block(self) -> None:
        m = lb.measure(job(102), (FIX / "failing.txt").read_bytes(), PATTERNS)
        worst = m.longest_failing()
        self.assertEqual(worst[0], "Compile (Windows)")
        self.assertEqual(worst[1].lines, 9)
        self.assertTrue(worst[1].errored)
        self.assertEqual(lb.check_targets(m, lb.load_baseline()), [])

    def test_oversized_failing_step_is_reported(self) -> None:
        line = "build (windows)\tCompile (Windows)\t2026-11-13T10:00:00Z " + "x" * 300
        raw = ((FIX / "failing.txt").read_text() + "\n".join([line] * 70) + "\n").encode()
        m = lb.measure(job(102), raw, PATTERNS)
        problems = lb.check_targets(m, lb.load_baseline())
        self.assertIn("build / build (windows)/Compile (Windows): expected <= 64 lines -> actual 79",
                      problems)
        self.assertTrue(any("expected <= 16384 bytes" in p for p in problems))

    def test_stray_line_in_green_step(self) -> None:
        extra = "build (linux)\tCompile\t2026-11-13T10:00:00Z -- Configuring done\n"
        raw = (FIX / "green.txt").read_bytes() + extra.encode()
        m = lb.measure(job(101), raw, PATTERNS)
        self.assertEqual(lb.check_targets(m, lb.load_baseline()),
                         ["build / build (linux)/Compile: expected <= 0 non-RC lines -> actual 1"])

    def test_green_job_above_baseline(self) -> None:
        m = lb.measure(job(101), (FIX / "green.txt").read_bytes(), PATTERNS)
        base = {"jobs": {"build (ubuntu-latest)": {"bytes": 100}},
                "leg_map": {"linux": "build (ubuntu-latest)"}}
        self.assertEqual(lb.check_targets(m, base),
                         [f"build / build (linux): expected bytes < 100 (build (ubuntu-latest)) "
                          f"-> actual {m.bytes}"])

    def test_log_without_step_column(self) -> None:
        m = lb.measure(job(103), (FIX / "nosplit.txt").read_bytes(), PATTERNS)
        self.assertFalse(m.split)
        self.assertIn("split unavailable", lb.summary_line(m, lb.load_baseline()))
        self.assertEqual(lb.check_targets(m, lb.load_baseline()), [])

    def test_empty_log(self) -> None:
        m = lb.measure(job(103), b"", PATTERNS)
        self.assertEqual((m.bytes, m.lines, m.split), (0, 0, False))

    def test_hostile_names_are_not_relayed(self) -> None:
        self.assertEqual(lb.safe_name("build (linux)"), "build (linux)")
        self.assertEqual(lb.safe_name("x\x1b]0;evil"), "<name>")
        self.assertEqual(lb.safe_name("::" + "error::x"), "<name>")


class RunTest(unittest.TestCase):
    """The whole run, through a fake gh."""

    def test_counts_only_never_log_text(self) -> None:
        summaries, problems = lb.run("43169714193", fake_runner(default_logs()),
                                     baseline=lb.load_baseline(), patterns=PATTERNS)
        self.assertEqual(problems, [])
        self.assertEqual(len(summaries), 3)
        self.assertTrue(summaries[0].startswith("build / build (linux) [success]: "))
        self.assertIn("baseline 413675 B", summaries[0])
        self.assertIn("longest failing step Compile (Windows): 9 lines", summaries[1])
        self.assertIn("split unavailable", summaries[2])
        joined = "\n".join(summaries)
        self.assertNotIn(SERIAL, joined)
        self.assertNotIn("undeclared", joined)

    def test_gh_called_with_expected_argv(self) -> None:
        calls: List[List[str]] = []
        inner = fake_runner(default_logs())

        def runner(args: Sequence[str]) -> bytes:
            calls.append(list(args))
            return inner(args)
        lb.run("7", runner, repo="o/r", baseline=lb.load_baseline(), patterns=PATTERNS)
        self.assertEqual(calls[0], ["run", "view", "7", "--json", "jobs", "--repo", "o/r"])
        self.assertEqual(calls[1], ["run", "view", "7", "--log", "--job", "101", "--repo", "o/r"])

    def test_bounded_discrepancies(self) -> None:
        lines = [f"j/s{i}: expected <= 64 lines -> actual 99" for i in range(23)]
        out = lb.bounded(lines)
        self.assertEqual(len(out), 21)
        self.assertEqual(out[-1], "(+3 more)")


class CliTest(unittest.TestCase):
    """The command line, with a stub gh process (argv, no shell)."""

    STUB = ("import sys, pathlib\n"
            "fix = pathlib.Path(sys.argv[1]); args = sys.argv[2:]\n"
            "if '--json' in args:\n"
            "    sys.stdout.buffer.write((fix / 'jobs.json').read_bytes())\n"
            "elif args[args.index('--job') + 1] == '102':\n"
            "    sys.exit(4)\n"
            "else:\n"
            "    sys.stdout.buffer.write((fix / 'green.txt').read_bytes())\n")

    def _main(self, *args: str) -> tuple:
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            rc = lb.main(list(args))
        return rc, out.getvalue(), err.getvalue()

    def test_gh_failure_exits_two(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            stub = Path(raw) / "gh_stub.py"
            stub.write_text(self.STUB, encoding="utf-8")
            rc, out, err = self._main("1", "--gh", sys.executable, "--gh", str(stub),
                                      "--gh", str(FIX))
        self.assertEqual(rc, 2)
        self.assertIn("gh exited 4", err)
        self.assertEqual(out, "")

    def test_bad_run_id_exits_two(self) -> None:
        rc, _, err = self._main("12;rm")
        self.assertEqual(rc, 2)
        self.assertIn("run id must be digits", err)

    def test_missing_gh_exits_two(self) -> None:
        rc, _, err = self._main("1", "--gh", "/nonexistent/gh-binary")
        self.assertEqual(rc, 2)
        self.assertIn("cannot run gh", err)


if __name__ == "__main__":
    unittest.main()
