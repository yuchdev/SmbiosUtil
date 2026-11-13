"""Tests for ``ctest_report.py`` (milestone 0014, Story 03.0 Task 02; plan E4, RC1/RC3/RC5, CR-9/CR-13).

Classification: unit tests of the JUnit parser and the discrepancy form (pure
functions over the ``fixtures/junit/`` files), plus step tests of
``run_set``/``main`` against a stub ``ctest`` on a temporary ``PATH`` that
prints a fixture log, copies a fixture JUnit file to ``--output-junit`` and
exits with a chosen code. No host ctest, no network, no firmware data.

Boundaries covered: all-pass, one-fail, one-timeout, all-skipped, not-run and
zero-test JUnit files; malformed, DTD-bearing, oversized, symlinked and
missing JUnit files (``counts unavailable``, the exit code decides); the
ctest 3.31 and 4.4 summary wordings (identical output); hostile
``system-out`` text; best-effort ``warn``; ``sudo -n`` argv (plan Q4).

On failure, first check: ``scripts/ci/test_sets.json`` (the verbatim
regexes) and ``fixtures/junit/README.md`` (what each fixture encodes); a
changed RC1 line shape usually means ``ci_step.run_step``'s ``finish`` hook
changed.
"""

from __future__ import annotations

import ast
import contextlib
import io
import json
import os
import re
import tempfile
import unittest
from pathlib import Path
from typing import List, Optional
from unittest import mock

from ci_test_support import CI_DIR, FIXTURES, path_with, write_stub

import ci_step  # noqa: E402
import ctest_report  # noqa: E402
from reporters import get_reporter  # noqa: E402

JUNIT = FIXTURES / "junit"
REPO_ROOT = CI_DIR.parents[1]
POSIX_ONLY = "stub commands are /bin/sh trampolines (POSIX only)"
UNPRIVILEGED_RE = ("smbios_unit_test|smbios_mock_test|smbios_c_|"
                   "smbios_cli_unknown_format|smbios_example_")
LIVE_TABLE_RE = "smbios_integration_test|smbios_perf_test|smbios_cli_format_csv"

#: Stub ctest: record argv, print STUB_LOG, copy STUB_JUNIT to --output-junit, exit STUB_RC.
STUB_CTEST = """\
import json, os, shutil, sys
with open(os.environ['STUB_ARGV'], 'a') as fh:
    fh.write(json.dumps(sys.argv[1:]) + '\\n')
log = os.environ.get('STUB_LOG')
if log:
    with open(log, 'rb') as fh:
        shutil.copyfileobj(fh, sys.stdout.buffer)
    sys.stdout.flush()
junit = os.environ.get('STUB_JUNIT')
if junit and '--output-junit' in sys.argv:
    shutil.copyfile(junit, sys.argv[sys.argv.index('--output-junit') + 1])
sys.exit(int(os.environ.get('STUB_RC', '0')))
"""

_ELAPSED = re.compile(r"\(\d+(?:\.\d+)?s\)")


class ParseJunitTests(unittest.TestCase):
    """``parse_junit``: counts from attributes only; anything doubtful is ``None``."""

    def counts(self, name: str) -> ctest_report.TestCounts:
        parsed = ctest_report.parse_junit(JUNIT / name)
        self.assertIsNotNone(parsed, name)
        return parsed[0]

    def test_AllPass(self) -> None:
        self.assertEqual(self.counts("all_pass.xml"), ctest_report.TestCounts(4, 0, 0, 0, 4))
        self.assertEqual(ctest_report.parse_junit(JUNIT / "all_pass.xml")[1], [])

    def test_OneFailWithHostileNameIsReplaced(self) -> None:
        counts, bad = ctest_report.parse_junit(JUNIT / "one_fail.xml")
        self.assertEqual(counts, ctest_report.TestCounts(1, 2, 0, 0, 3))
        self.assertEqual(bad, [("synthetic_mock_test", "failed"), ("<test #3>", "failed")])

    def test_TimeoutIsAFailureWithItsOwnStatus(self) -> None:
        counts, bad = ctest_report.parse_junit(JUNIT / "one_timeout.xml")
        self.assertEqual((counts.passed, counts.failed, counts.total), (1, 1, 2))
        self.assertEqual(bad, [("synthetic_perf_test", "timeout")])

    def test_SkippedAndDisabledAreSkipsNotDiscrepancies(self) -> None:
        counts, bad = ctest_report.parse_junit(JUNIT / "all_skipped.xml")
        self.assertEqual(counts, ctest_report.TestCounts(0, 0, 2, 0, 2))
        self.assertEqual(bad, [])

    def test_NotRunIsCountedAndReported(self) -> None:
        counts, bad = ctest_report.parse_junit(JUNIT / "not_run.xml")
        self.assertEqual(counts, ctest_report.TestCounts(0, 0, 0, 1, 1))
        self.assertEqual(bad, [("t_notrun", "notrun")])

    def test_ZeroTests(self) -> None:
        self.assertEqual(self.counts("zero.xml"), ctest_report.TestCounts(0, 0, 0, 0, 0))

    def test_DoctypeMalformedAndMissingAreUnavailable(self) -> None:
        for name in ("doctype.xml", "malformed.xml", "does_not_exist.xml"):
            with self.subTest(fixture=name):
                self.assertIsNone(ctest_report.parse_junit(JUNIT / name))

    def test_EntityAfterFirst4KiBIsStillRefused(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "late.xml"
            path.write_text('<?xml version="1.0"?>\n<!--' + "x" * 5000 + '-->\n'
                            '<!DOCTYPE t [<!ENTITY a "b">]><testsuite/>\n', encoding="utf-8")
            self.assertIsNone(ctest_report.parse_junit(path))

    def test_OversizedIsUnavailable(self) -> None:
        self.assertIsNone(ctest_report.parse_junit(JUNIT / "all_pass.xml", max_bytes=100))
        with tempfile.TemporaryDirectory() as tmp:
            big = Path(tmp) / "big.xml"
            with open(big, "wb") as fh:  # sparse: size above the 32 MiB cap, nothing read
                fh.truncate(ctest_report.JUNIT_MAX_BYTES + 1)
            self.assertIsNone(ctest_report.parse_junit(big))

    @unittest.skipIf(os.name == "nt", "symlink creation needs privileges on Windows")
    def test_SymlinkIsUnavailable(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            link = Path(tmp) / "junit.xml"
            link.symlink_to(JUNIT / "all_pass.xml")
            self.assertIsNone(ctest_report.parse_junit(link))

    def test_UnknownStatusFailsClosedAndTestsuitesRootIsAccepted(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "j.xml"
            path.write_text('<testsuites><testsuite><testcase name="a" status="weird"/>'
                            '<testcase name="b" status="run"/></testsuite></testsuites>',
                            encoding="utf-8")
            counts, bad = ctest_report.parse_junit(path)
            self.assertEqual((counts.passed, counts.failed), (1, 1))
            self.assertEqual(bad, [("a", "failed")])
            path.write_text("<html/>", encoding="utf-8")
            self.assertIsNone(ctest_report.parse_junit(path))


class DiscrepancyTests(unittest.TestCase):
    """RC1 discrepancy form: at most 20 lines plus ``(+k more)``, 160 characters each."""

    def test_CapAndMore(self) -> None:
        bad = [(f"t{i}", "failed") for i in range(23)]
        lines = ctest_report.discrepancy_lines(bad)
        self.assertEqual(len(lines), 21)
        self.assertEqual(lines[0], "t0: expected passed -> failed")
        self.assertEqual(lines[-1], "(+3 more)")

    def test_LineLength(self) -> None:
        (line,) = ctest_report.discrepancy_lines([("a" * 120, "timeout")])
        self.assertLessEqual(len(line), 160)

    def test_UnavailableSummary(self) -> None:
        fin = ctest_report.summarize(None)
        self.assertEqual(fin.summary, "counts unavailable")
        self.assertIsNone(fin.counts)


class SourceTests(unittest.TestCase):
    """Plan E4: nothing in ctest_report.py matches ctest's human summary wording."""

    def test_NoSummaryLineParsing(self) -> None:
        tree = ast.parse((CI_DIR / "ctest_report.py").read_text(encoding="utf-8"))
        patterns = [node.args[0].value for node in ast.walk(tree)
                    if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                    and isinstance(node.func.value, ast.Name) and node.func.value.id == "re"
                    and node.args and isinstance(node.args[0], ast.Constant)]
        self.assertTrue(patterns, "expected the allowlist regexes")
        for pattern in patterns:
            for needle in ("passed", "Total", "failed", "tests", "%"):
                self.assertNotIn(needle, pattern)

    def test_SetsAreVerbatimCopiesOfTheOldWorkflow(self) -> None:
        timeout, sets = ctest_report.load_sets()
        self.assertEqual(timeout, "60")
        self.assertEqual(sets["unprivileged"].args, ("-R", UNPRIVILEGED_RE))
        self.assertEqual(sets["live_table"].args, ("-R", LIVE_TABLE_RE))
        self.assertEqual(sets["gui_ci"].args, ("-L", "gui-ci"))
        old = (REPO_ROOT / ".github" / "workflows" / "ci.yml")
        if old.exists():  # deleted at the Task 04 cutover
            text = old.read_text(encoding="utf-8")
            self.assertIn(UNPRIVILEGED_RE, text)
            self.assertIn(LIVE_TABLE_RE, text)
        self.assertTrue(sets["live_table"].live_table)
        self.assertFalse(sets["live_table"].output_on_failure)

    def test_MalformedSetsFileIsAUsageError(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sets.json"
            for body in ('{"timeout": "60", "sets": {"x": {"args": ["-V", "y"], '
                         '"output_on_failure": true, "live_table": false}}}',
                         '{"timeout": 60, "sets": {}}', "not json"):
                with self.subTest(body=body):
                    path.write_text(body, encoding="utf-8")
                    with self.assertRaises(ctest_report.UsageError):
                        ctest_report.load_sets(path)


@unittest.skipIf(os.name == "nt", POSIX_ONLY)
class RunSetTests(unittest.TestCase):
    """``run_set`` against a stub ctest: exit code decides, counts from JUnit."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        self.bin = self.tmp / "bin"
        write_stub(self.bin, "ctest", STUB_CTEST)
        self.argv_file = self.tmp / "argv.json"
        self._env = mock.patch.dict(os.environ, {
            "PATH": path_with(self.bin), "STUB_ARGV": str(self.argv_file),
            "CI_RESULTS_FILE": str(self.tmp / "results.jsonl"),
            "CI_LOG_DIR": str(self.tmp / "logs"), "CI_TEMP_DIR": str(self.tmp)})
        self._env.start()

    def tearDown(self) -> None:
        self._env.stop()
        self._tmp.cleanup()

    def run_set(self, set_name: str = "unprivileged", *, junit: Optional[str] = None,
                log: Optional[str] = None, rc: int = 0, reporter: str = "github",
                best_effort: Optional[str] = None, config: Optional[str] = None,
                label: Optional[str] = None, sudo: bool = False) -> tuple:
        env = {"STUB_RC": str(rc), "STUB_JUNIT": str(JUNIT / junit) if junit else "",
               "STUB_LOG": str(FIXTURES / log) if log else ""}
        out = io.StringIO()
        with mock.patch.dict(os.environ, env), contextlib.redirect_stdout(out):
            code = ctest_report.run_set(set_name, self.tmp / "build", config,
                                        label or set_name.replace("_", "-"), best_effort,
                                        sudo=sudo, reporter=get_reporter(reporter))
        return code, out.getvalue().splitlines()

    def records(self) -> List[dict]:
        path = self.tmp / "results.jsonl"
        return [json.loads(x) for x in path.read_text(encoding="ascii").splitlines()]

    def argvs(self) -> List[List[str]]:
        return [json.loads(x) for x in self.argv_file.read_text().splitlines()]

    def test_AllPassIsOneOkLineWithJUnitCounts(self) -> None:
        rc, lines = self.run_set(junit="all_pass.xml", log="ctest_31_pass.log", config="Debug")
        self.assertEqual(rc, 0)
        self.assertEqual(len(lines), 1)
        self.assertRegex(lines[0], r"^ok: tests unprivileged: 4 passed, 0 skipped, 4 total "
                                   r"\(\d+(\.\d+)?s\)$")
        (record,) = self.records()
        self.assertEqual(record["counts"], {"passed": 4, "failed": 0, "skipped": 0, "not_run": 0})
        (argv,) = self.argvs()
        self.assertEqual(argv[argv.index("-R") + 1], UNPRIVILEGED_RE)
        self.assertEqual(argv[argv.index("-C") + 1], "Debug")
        self.assertEqual(argv[argv.index("--timeout") + 1], "60")
        self.assertEqual(argv[argv.index("--output-junit") + 1],
                         str(self.tmp / "junit-unprivileged.xml"))
        self.assertIn("--no-tests=error", argv)
        self.assertIn("--output-on-failure", argv)

    def test_Ctest3And4WordingsGiveIdenticalOutput(self) -> None:
        outputs = []
        for log in ("ctest_31_pass.log", "ctest_44_pass.log"):
            _, lines = self.run_set(junit="all_pass.xml", log=log)
            outputs.append([_ELAPSED.sub("(Xs)", x) for x in lines])
        self.assertEqual(outputs[0], outputs[1])
        self.assertEqual(outputs[0], ["ok: tests unprivileged: 4 passed, 0 skipped, 4 total (Xs)"])

    def test_OneFailPrintsDiscrepanciesAndNeverSystemOut(self) -> None:
        rc, lines = self.run_set(junit="one_fail.xml", rc=8)
        self.assertEqual(rc, 8)
        text = "\n".join(lines)
        self.assertTrue(lines[0].startswith("::error title=tests unprivileged::"))
        self.assertIn("synthetic_mock_test: expected passed -> failed", lines[0])
        self.assertIn("synthetic_mock_test: expected passed -> failed", lines)
        self.assertIn("<test #3>: expected passed -> failed", lines)
        for secret in ("injected from system-out", "TEST-SERIAL-0000", "::warning::x"):
            self.assertNotIn(secret, text)
        (record,) = self.records()
        self.assertEqual((record["outcome"], record["exit"]), ("fail", 8))
        self.assertEqual(record["counts"], {"passed": 1, "failed": 2, "skipped": 0, "not_run": 0})
        self.assertNotIn("TEST-SERIAL-0000", json.dumps(record))

    def test_SystemOutCommandNeverReachesStdoutOnSuccess(self) -> None:
        _, lines = self.run_set(junit="all_pass.xml")
        self.assertNotIn("::error::", "\n".join(lines))
        self.assertNotIn("::add-mask::", "\n".join(lines))

    def test_TimeoutDiscrepancy(self) -> None:
        rc, lines = self.run_set(junit="one_timeout.xml", rc=8)
        self.assertEqual(rc, 8)
        self.assertIn("synthetic_perf_test: expected passed -> timeout", lines)

    def test_AllSkippedPasses(self) -> None:
        rc, lines = self.run_set(junit="all_skipped.xml")
        self.assertEqual(rc, 0)
        self.assertRegex(lines[0], r"^ok: tests unprivileged: 0 passed, 2 skipped, 2 total ")

    def test_ZeroTestsFailsTheGuiSet(self) -> None:
        rc, lines = self.run_set("gui_ci", junit="zero.xml", rc=8)
        self.assertEqual(rc, 8)
        self.assertTrue(lines[0].startswith("::error title=tests gui-ci::"))
        (argv,) = self.argvs()
        self.assertEqual(argv[argv.index("-L") + 1], "gui-ci")
        self.assertIn("--no-tests=error", argv)
        self.assertEqual(self.records()[0]["counts"]["passed"], 0)

    def test_CountsUnavailableExitCodeDecides(self) -> None:
        cases = ((None, 0, "ok"), ("malformed.xml", 0, "ok"), ("doctype.xml", 8, "fail"),
                 ("malformed.xml", 8, "fail"))
        for junit, code, outcome in cases:
            with self.subTest(junit=junit, rc=code):
                (self.tmp / "results.jsonl").unlink(missing_ok=True)
                rc, lines = self.run_set(junit=junit, rc=code)
                self.assertEqual(rc, code)
                (record,) = self.records()
                self.assertEqual(record["outcome"], outcome)
                self.assertIsNone(record["counts"])
                if outcome == "ok":
                    self.assertRegex(lines[0], r"^ok: tests unprivileged: counts unavailable ")
                else:
                    self.assertTrue(any("counts unavailable" in x for x in lines))

    def test_ExitCodeDecidesOverJUnit(self) -> None:
        rc, lines = self.run_set(junit="all_pass.xml", rc=8)
        self.assertEqual(rc, 8)
        self.assertTrue(lines[0].startswith("::error title=tests unprivileged::"))

    def test_StaleJUnitIsNeverReported(self) -> None:
        stale = self.tmp / "junit-unprivileged.xml"
        stale.write_bytes((JUNIT / "one_fail.xml").read_bytes())
        _, lines = self.run_set(junit=None, rc=0)
        self.assertRegex(lines[0], r"^ok: tests unprivileged: counts unavailable ")
        self.assertFalse(stale.exists())

    def test_SymlinkedJUnitPathIsRefused(self) -> None:
        target = self.tmp / "elsewhere.xml"
        target.write_text("keep", encoding="utf-8")
        (self.tmp / "junit-unprivileged.xml").symlink_to(target)
        with self.assertRaises(ci_step.UnsafePathError):
            self.run_set(junit="all_pass.xml")
        self.assertEqual(target.read_text(encoding="utf-8"), "keep")

    def test_BestEffortLiveTableWarnsCountsOnly(self) -> None:
        rc, lines = self.run_set("live_table", junit="one_fail.xml", rc=8,
                                 log="ctest_31_pass.log", best_effort="firmware may hide it")
        self.assertEqual(rc, 0, "best-effort never fails the step (RC5)")
        text = "\n".join(lines)
        self.assertTrue(lines[0].startswith("::warning title=best-effort: tests live-table::"))
        self.assertIn("synthetic_mock_test: expected passed -> failed", lines)
        self.assertNotIn("Test project", text, "counts-only shows CTest status lines only")
        self.assertNotIn("TEST-SERIAL-0000", text)
        (record,) = self.records()
        self.assertEqual((record["mode"], record["outcome"]), ("best-effort", "warn"))
        self.assertEqual(record["counts"]["failed"], 2)
        self.assertTrue((self.tmp / "logs" / "live" / "tests_live-table.log").exists())
        (argv,) = self.argvs()
        self.assertNotIn("--output-on-failure", argv)
        self.assertEqual(argv[argv.index("-R") + 1], LIVE_TABLE_RE)

    def test_SudoRunsAbsoluteCtestNonInteractively(self) -> None:
        write_stub(self.bin, "sudo", "import json, os, sys\n"
                   "with open(os.environ['STUB_ARGV'], 'a') as fh:\n"
                   "    fh.write(json.dumps(sys.argv[1:]) + '\\n')\n")
        rc, _ = self.run_set("live_table", sudo=True, best_effort="x")
        self.assertEqual(rc, 0)
        (argv,) = self.argvs()
        self.assertEqual(argv[0], "-n")
        self.assertTrue(os.path.isabs(argv[1]) and argv[1].endswith("ctest"))
        self.assertEqual(argv[2], "--test-dir")

    def test_UsageErrors(self) -> None:
        for kwargs in ({"set_name": "nope"}, {"label": "Bad Label"}, {"config": "a;b"}):
            with self.subTest(**kwargs), self.assertRaises(ctest_report.UsageError):
                self.run_set(**kwargs)


class MainTests(unittest.TestCase):
    """Command line: usage errors exit 2 without a traceback."""

    def test_UnknownSetExits2(self) -> None:
        err = io.StringIO()
        with tempfile.TemporaryDirectory() as tmp, contextlib.redirect_stderr(err), \
                mock.patch.dict(os.environ, {"CI_TEMP_DIR": tmp, "CI_LOG_DIR": tmp,
                                             "CI_RESULTS_FILE": str(Path(tmp) / "r.jsonl")}):
            rc = ctest_report.main(["--set", "nope", "--test-dir", tmp, "--reporter", "plain"])
        self.assertEqual(rc, 2)
        self.assertIn("set must be one of: gui_ci, live_table, unprivileged", err.getvalue())


if __name__ == "__main__":
    unittest.main()
