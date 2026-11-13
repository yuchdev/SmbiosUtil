"""Tests for ``parity_check.py`` and ``inventory.json`` (milestone 0014, Story 03.0 Task 04).

Classification: unit tests (no network, no host cmake/ctest, nothing executed:
``parity_check`` dry-runs ``stage.py`` in-process with every side effect
recorded). Scenarios:

- **contract**: ``inventory.json`` has exactly plan.md's C01-C22 IDs, Gate
  column (verbatim and normalised) and dispositions;
- **normal**: today's tree is at parity (one summary line, exit 0);
- **negative proofs of the gate itself**: each test breaks one thing in a
  copy of the workflows (or patches one stage step) and asserts the exact
  discrepancy line - a removed Windows test step, a leg dropped from a
  matrix, a hard step turned best-effort, a failure swallowed, a job budget
  over the 30-minute ceiling, an unpinned action, a checkout that keeps its
  credentials, the dropped C05 token reintroduced, an expected gate stronger
  than the implemented one;
- **edges**: output bounded to 20 lines plus ``(+k more)``; malformed
  inventory exits 2; the dry run restores every patched function and
  environment variable.

On failure, first check: whether ``stage.py``'s step lists (``JOB_STEPS``) or
a stage workflow's ``run:`` lines changed - the ``--table`` output of
``python3 scripts/ci/parity_check.py --table`` shows the observed mapping per
row; then ``inventory.json``'s ``checks`` for the row named in the failure.
"""

from __future__ import annotations

import contextlib
import io
import json
import os
import re
import shutil
import tempfile
import unittest
from pathlib import Path
from typing import Callable, List, Optional
from unittest import mock

from ci_test_support import CI_DIR

import ci_step  # noqa: E402
import gui_guards  # noqa: E402
import parity_check as pc  # noqa: E402
import stage  # noqa: E402

PLAN = CI_DIR.parents[1] / "docs" / "roadmap" / "0014-ci-rework-from-scratch" / "plan.md"
CLEAN = "inventory parity: 21 of 21 kept, 1 dropped (C05: dead)"
_PLAN_ROW = re.compile(r"^\|\s*(C\d\d)\s*\|")


def plan_rows() -> List[List[str]]:
    """Return the cells of plan.md's Phase-0 inventory rows."""
    rows = []
    for line in PLAN.read_text(encoding="utf-8").splitlines():
        if _PLAN_ROW.match(line):
            # Split on unescaped pipes only (cells hold `a\|b` regexes).
            rows.append([c.strip() for c in re.split(r"(?<!\\)\|", line.strip().strip("|"))])
    return rows


class InventoryContractTest(unittest.TestCase):
    """inventory.json is plan.md's table, row for row."""

    def test_ids_gates_and_dispositions_match_plan(self) -> None:
        rows = {r["id"]: r for r in pc.load_inventory()}
        plan = plan_rows()
        self.assertEqual([p[0] for p in plan], [f"C{n:02d}" for n in range(1, 23)])
        self.assertEqual(sorted(rows), [p[0] for p in plan])
        for cells in plan:
            rid, gate, disposition = cells[0], cells[3], cells[4]
            with self.subTest(row=rid):
                self.assertEqual(rows[rid]["plan_gate"], gate)
                self.assertEqual(rows[rid]["gate"], gate.split(" ", 1)[0])
                want = "drop" if disposition.startswith("**Drop**") else "keep"
                self.assertEqual(rows[rid]["disposition"], want)

    def test_dropped_rows_give_a_reason(self) -> None:
        for row in pc.load_inventory():
            if row["disposition"] == "drop":
                self.assertTrue(row.get("reason"), row["id"])


class _TreeCase(unittest.TestCase):
    """Helpers: a temporary copy of the workflows, edited per test."""

    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp(prefix="parity-test-"))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.wf = self.tmp / "workflows"
        self.wf.mkdir()
        for path in pc.WORKFLOWS_DIR.glob("*.yml"):
            if path.name != "ci.yml":
                shutil.copy(path, self.wf / path.name)

    def edit(self, name: str, fn: Callable[[str], str]) -> None:
        path = self.wf / name
        before = path.read_text(encoding="utf-8")
        after = fn(before)
        self.assertNotEqual(before, after, f"edit of {name} changed nothing")
        path.write_text(after, encoding="utf-8")

    def report(self, inventory: Optional[Path] = None) -> List[str]:
        findings, extra, rows = pc.check(pc.Tree(workflows_dir=self.wf),
                                         inventory or pc.INVENTORY_FILE)
        lines, _ = pc.render(findings, rows, extra)
        return lines


def drop_step(text: str, name: str) -> str:
    """Remove the step ``- name: <name>`` (up to the next blank line) from a workflow."""
    return re.sub(rf"\n      - name: {re.escape(name)}\n(?:.+\n)+", "\n", text, count=1)


class ParityTest(_TreeCase):
    """The gate on today's tree and on deliberately broken copies."""

    def test_todays_tree_is_at_parity(self) -> None:
        self.assertEqual(self.report(), [CLEAN])

    def test_cli_prints_one_line_and_exits_zero(self) -> None:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = pc.main(["--workflows-dir", str(self.wf)])
        self.assertEqual(rc, 0)
        self.assertEqual(out.getvalue().splitlines(), [CLEAN])

    def test_removed_windows_test_step_is_missing(self) -> None:
        self.edit("stage-build.yml", lambda t: drop_step(t, "Test (Windows)"))
        lines = self.report()
        self.assertIn("C06: expected hard -> actual missing (windows)", lines)
        self.assertIn("C10: expected best-effort -> actual missing (windows)", lines)

    def test_leg_dropped_from_matrix_is_missing(self) -> None:
        self.edit("stage-gui.yml", lambda t: t.replace(', "macos-gui"]', "]"))
        lines = self.report()
        for rid in ("C16", "C17", "C18", "C19", "C20"):
            self.assertIn(f"{rid}: expected hard -> actual missing (macos-gui)", lines)

    def test_hard_step_turned_best_effort_is_weaker(self) -> None:
        def unit(ctx: stage.Context) -> int:
            return stage._run_set(ctx, "unprivileged", "unprivileged", "made best-effort")
        with mock.patch.dict(stage.STEP_FUNCTIONS["test"], {"unit": unit}):
            lines = self.report()
        self.assertIn("C06: expected hard -> actual best-effort (windows)", lines)
        self.assertIn("C13: expected hard -> actual best-effort (linux-asan,macos-asan)", lines)

    def test_swallowed_failure_is_ignored(self) -> None:
        def hooks(ctx: stage.Context) -> int:
            gui_guards.check_hook_strings([], None)
            return 0
        with mock.patch.dict(stage.STEP_FUNCTIONS["test"], {"gui-hooks": hooks}):
            lines = self.report()
        self.assertEqual(lines,
                         ["C18: expected hard -> actual ignored (linux-gui,macos-gui,windows-gui)"])

    def test_job_over_ceiling_breaks_c11(self) -> None:
        self.edit("stage-build.yml", lambda t: t.replace("timeout-minutes: 18", "timeout-minutes: 45"))
        self.assertIn("C11: expected hard -> actual missing (stage-build.yml/build job timeout)",
                      self.report())

    def test_unpinned_action_breaks_c21(self) -> None:
        self.edit("stage-lint.yml", lambda t: re.sub(r"actions/checkout@[0-9a-f]{40}",
                                                     "actions/checkout@v7", t))
        self.assertIn("C21: expected policy -> actual missing (P2 violated)", self.report())

    def test_checkout_keeping_credentials_is_reported(self) -> None:
        self.edit("stage-lint.yml", lambda t: t.replace("persist-credentials: false",
                                                        "persist-credentials: true"))
        self.assertIn("P14 stage-lint.yml/lint: expected persist-credentials false -> actual true",
                      self.report())

    def test_dropped_token_reintroduced(self) -> None:
        self.edit("stage-build.yml", lambda t: t + "# runs smbios_functional_test\n")
        self.assertIn("C05: expected dropped -> actual referenced (stage-build.yml)",
                      self.report())

    def test_expected_gate_stronger_than_implemented(self) -> None:
        data = json.loads(pc.INVENTORY_FILE.read_text(encoding="utf-8"))
        for row in data["rows"]:
            if row["id"] == "C10":
                row["gate"] = "hard"
        inv = self.tmp / "inventory.json"
        inv.write_text(json.dumps(data), encoding="utf-8")
        self.assertEqual(self.report(inv), ["C10: expected hard -> actual best-effort (windows)"])


class EdgeTest(unittest.TestCase):
    """Bounds, usage errors and side-effect hygiene."""

    def test_output_bounded_to_twenty_lines(self) -> None:
        findings = [pc.Finding(f"C{n:02d}", "hard", "missing") for n in range(25)]
        lines, ok = pc.render(findings, [])
        self.assertFalse(ok)
        self.assertEqual(len(lines), 21)
        self.assertEqual(lines[-1], "(+5 more)")

    def test_stronger_actual_gate_passes(self) -> None:
        self.assertFalse(pc.is_discrepancy(pc.Finding("C09", "info", "hard")))
        self.assertTrue(pc.is_discrepancy(pc.Finding("C03", "infra", "hard")))

    def test_malformed_inventory_exits_two(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            bad = Path(raw) / "inventory.json"
            bad.write_text('{"rows": [{"id": "C01"}]}', encoding="utf-8")
            err = io.StringIO()
            with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()):
                rc = pc.main(["--inventory", str(bad)])
        self.assertEqual(rc, 2)
        self.assertIn("every row needs", err.getvalue())

    def test_dry_run_restores_everything(self) -> None:
        before_env = {k: os.environ.get(k) for k in ("CI_LOG_DIR", "CI_REPORTER", "ASAN_OPTIONS")}
        run_step, skip = ci_step.run_step, stage.Context.skip
        pc.check()
        self.assertIs(ci_step.run_step, run_step)
        self.assertIs(stage.Context.skip, skip)
        self.assertEqual(before_env,
                         {k: os.environ.get(k) for k in ("CI_LOG_DIR", "CI_REPORTER",
                                                          "ASAN_OPTIONS")})

    def test_step_if_evaluation(self) -> None:
        self.assertTrue(pc.step_applies(None, "linux"))
        self.assertTrue(pc.step_applies("matrix.leg != 'windows'", "linux"))
        self.assertFalse(pc.step_applies("matrix.leg != 'windows'", "windows"))
        self.assertTrue(pc.step_applies("matrix.leg == 'linux-gui'", "linux-gui"))
        self.assertFalse(pc.step_applies("matrix.leg == 'linux-gui'", "macos-gui"))


if __name__ == "__main__":
    unittest.main()
