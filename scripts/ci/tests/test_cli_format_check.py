"""Tests for ``cli_format_check.py`` (milestone 0014, Story 03.0 Task 01; inventory C07-C09).

Classification: integration-style tests of the script against a **stub**
``smbios`` CLI (a tiny Python program behind a ``/bin/sh`` trampoline at
``<build>/smbios/bin/smbios``) and the real
``examples/scripts/check_structured_output.py``; plus unit tests of the
parsing helpers and of the reporters' ``warning()`` event. No host CLI, no
firmware data: the stub's "serial" is the fake ``TEST-SERIAL-0000``.

Boundaries: the stub prints hostile workflow-command and service-message
text on stdout and stderr; none of it may reach this script's output or the
RC3 records (threat model CR-4, CR-9).

On failure, first check: the checker's own ``ok:``/``FAIL:`` wording in
``examples/scripts/check_structured_output.py`` against
``cli_format_check.OK_COUNT_LINE``/``OK_TEXT_LINE``/``FAIL_REASONS``.
"""

from __future__ import annotations

import contextlib
import io
import os
import tempfile
import unittest
from pathlib import Path
from typing import List, Tuple
from unittest import mock

from ci_test_support import write_stub

import ci_step  # noqa: E402
import cli_format_check as cfc  # noqa: E402
from reporters import get_reporter  # noqa: E402

POSIX_ONLY = "the stub CLI is a /bin/sh trampoline (POSIX only)"
SERIAL = "TEST-SERIAL-0000"

#: Stub ``smbios`` CLI. STUB_COUNT structures; STUB_NOFLAG_DIFF makes the
#: no-flag output differ from --format text. Every document carries the fake
#: serial, and stderr carries hostile lines.
STUB_CLI = r'''
import os, sys
n = int(os.environ.get("STUB_COUNT", "3"))
fmt = sys.argv[sys.argv.index("--format") + 1] if "--format" in sys.argv else None
sys.stderr.write("::error::pwned from stderr\n##teamcity[buildStatus text='green']\n"
                 "ok: json, 999 structures\nFAIL: TEST-SERIAL-0000\n")
serial = "TEST-SERIAL-0000"
if fmt == "json":
    body = "[" + ",".join('{"type": "BIOS", "fields": {"Serial": "%s"}}' % serial
                          for _ in range(n)) + "]"
elif fmt == "xml":
    body = "<smbios>" + "".join('<entry type="BIOS"><field name="Serial">%s</field></entry>'
                                % serial for _ in range(n)) + "</smbios>"
elif fmt == "yaml":
    body = "---\n" + "".join('- type: "BIOS"\n  Serial: "%s"\n' % serial for _ in range(n))
elif fmt == "csv":
    body = "type,field_name,field_value\n" + "".join("BIOS,Serial,%s\n" % serial
                                                     for _ in range(n))
else:
    body = "".join("BIOS Information\n\tSerial: %s ::error::x\n" % serial for _ in range(n))
    body = body or "No SMBIOS table\n"
    if fmt is None and os.environ.get("STUB_NOFLAG_DIFF"):
        body += "extra\n"
sys.stdout.write(body)
'''


class _Base(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        self.build = self.tmp / "build" / "linux"
        self.log_dir = self.tmp / "logs"
        self.results = self.tmp / "results.jsonl"
        self._env = mock.patch.dict(os.environ, {"STUB_COUNT": "3"})
        self._env.start()
        self.addCleanup(self._env.stop)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def install_cli(self) -> Path:
        bindir = self.build / "smbios" / "bin"
        write_stub(bindir, "smbios", STUB_CLI)
        return bindir / "smbios"

    def ctx(self, backend: str = "github") -> cfc._Ctx:
        return cfc._Ctx(get_reporter(backend), self.log_dir, self.results)

    def run_check(self, backend: str = "github", **kw) -> Tuple[int, List[str], str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = cfc.run(self.build, ctx=self.ctx(backend), **kw)
        text = out.getvalue()
        records = self.results.read_text(encoding="ascii") if self.results.exists() else ""
        return rc, text.splitlines(), records

    def assertNothingLeaked(self, text: str, records: str) -> None:
        for needle in (SERIAL, "::error::", "pwned", "999"):
            self.assertNotIn(needle, text)
            self.assertNotIn(needle, records)


@unittest.skipIf(os.name == "nt", POSIX_ONLY)
class StubCliTests(_Base):
    """The five formats, empty tables, no-flag mismatch, missing binary."""

    def test_FiveFormatsWithCountsGiveOneOkLine(self) -> None:
        self.install_cli()
        rc, lines, records = self.run_check()
        self.assertEqual(rc, 0, lines)
        self.assertEqual(len(lines), 1, lines)
        self.assertRegex(lines[0], r"^ok: cli formats: text non-empty, xml 3, json 3, yaml 3, "
                                   r"csv 3; no-flag == text \(\d+\.\ds\)$")
        self.assertNothingLeaked("\n".join(lines), records)

    def test_EmptyTableGivesFiveWarningsAndStillOk(self) -> None:
        self.install_cli()
        with mock.patch.dict(os.environ, {"STUB_COUNT": "0"}):
            rc, lines, records = self.run_check()
        self.assertEqual(rc, 0, lines)
        self.assertEqual(len(lines), 6, lines)
        warnings = lines[:5]
        for fmt, line in zip(cfc.FORMATS, warnings):
            self.assertTrue(line.startswith("::warning title=empty table::smbios --format "
                                            f"{fmt}: well-formed but empty document"), line)
        self.assertTrue(lines[5].startswith("ok: cli formats: text non-empty, xml 0, json 0, "
                                            "yaml 0, csv 0; no-flag == text"))
        self.assertNothingLeaked("\n".join(lines), records)

    def test_NoFlagMismatchIsAHardFailureWithByteCounts(self) -> None:
        self.install_cli()
        with mock.patch.dict(os.environ, {"STUB_NOFLAG_DIFF": "1"}):
            rc, lines, records = self.run_check()
        self.assertEqual(rc, 1)
        self.assertRegex(lines[0], r"^::error title=cli formats::cli formats failed \(exit 1, "
                                   r"[0-9.]+s\): no-flag output: expected == --format text -> "
                                   r"actual differs \(\d+ vs \d+ bytes\)$")
        self.assertNothingLeaked("\n".join(lines), records)

    def test_MissingBinaryIsAHardFailure(self) -> None:
        self.build.mkdir(parents=True)
        rc, lines, _ = self.run_check(backend="plain")
        self.assertEqual(rc, 1)
        self.assertIn("smbios CLI binary not found", lines[0])

    def test_FailingCliShowsOnlyTheFixedReason(self) -> None:
        bindir = self.build / "smbios" / "bin"
        write_stub(bindir, "smbios", "import sys\nsys.stderr.write('TEST-SERIAL-0000\\n')\n"
                                     "sys.exit(3)\n")
        rc, lines, records = self.run_check()
        self.assertEqual(rc, 1)
        text = "\n".join(lines)
        self.assertIn("smbios --format text: command exited with status 3", text)
        self.assertIn("no-flag output: comparison could not run", text)
        self.assertNothingLeaked(text, records)

    def test_EveryLiveTableChildRunsCountsOnly(self) -> None:
        self.install_cli()
        real = ci_step.run_step
        with mock.patch("ci_step.run_step", side_effect=real) as spy:
            rc, _, _ = self.run_check()
        self.assertEqual(rc, 0)
        self.assertEqual(spy.call_count, len(cfc.FORMATS) + 1)
        for call in spy.call_args_list:
            self.assertEqual(call.kwargs.get("hygiene"), "counts-only", call.args[0])
        live = sorted(p.name for p in (self.log_dir / "live").iterdir())
        self.assertIn("cli_no-flag_compare.log", live)
        self.assertEqual(sorted(p.name for p in self.log_dir.iterdir()), ["live"])

    def test_TeamcityBackendRendersNoForeignServiceMessage(self) -> None:
        self.install_cli()
        rc, lines, records = self.run_check(backend="teamcity")
        self.assertEqual(rc, 0)
        self.assertNotIn("##teamcity[", "\n".join(lines))
        self.assertNothingLeaked("\n".join(lines), records)


@unittest.skipIf(os.name == "nt", POSIX_ONLY)
class StubCheckerTests(_Base):
    """The checker's own lines are parsed and re-rendered, never relayed."""

    def checker(self, body: str) -> Path:
        path = self.tmp / "checker.py"
        path.write_text("import sys\nfmt = sys.argv[sys.argv.index('--format') + 1]\n" + body,
                        encoding="utf-8")
        return path

    def test_HostileSuffixAfterTheCountIsDropped(self) -> None:
        self.install_cli()
        stub = self.checker(
            "if fmt == 'text':\n"
            "    print(\"ok: text, non-empty ##teamcity[buildStatus text='green']\")\n"
            "else:\n"
            "    unit = 'rows' if fmt == 'csv' else 'structures'\n"
            "    print(f\"ok: {fmt}, 7 {unit} ##teamcity[buildStatus text='green'] TEST-SERIAL-0000\")\n")
        with mock.patch.object(cfc, "CHECK_SCRIPT", stub):
            rc, lines, records = self.run_check(backend="plain")
        self.assertEqual(rc, 0, lines)
        text = "\n".join(lines)
        self.assertIn("xml 7, json 7, yaml 7, csv 7", text)
        self.assertNotIn("##teamcity[", text)
        self.assertNothingLeaked(text, records)

    def test_UnparseableLineIsAHardFailure(self) -> None:
        self.install_cli()
        stub = self.checker("print(f'ok: {fmt}, lots of structures TEST-SERIAL-0000')\n")
        with mock.patch.object(cfc, "CHECK_SCRIPT", stub):
            rc, lines, records = self.run_check(backend="plain")
        self.assertEqual(rc, 1)
        self.assertIn("unparseable checker result line", lines[0])
        self.assertNothingLeaked("\n".join(lines), records)

    def test_WrongUnitOrFormatIsUnparseable(self) -> None:
        self.install_cli()
        stub = self.checker("print('ok: xml, 4 structures')\n")
        with mock.patch.object(cfc, "CHECK_SCRIPT", stub):
            rc, lines, _ = self.run_check(backend="plain")
        self.assertEqual(rc, 1)
        self.assertIn("smbios --format text: unparseable", lines[0])

    def test_UnknownFailReasonIsReplacedByAFixedOne(self) -> None:
        self.install_cli()
        stub = self.checker("sys.stderr.write('FAIL: TEST-SERIAL-0000 ::error::x\\n')\n"
                            "sys.exit(1)\n")
        with mock.patch.object(cfc, "CHECK_SCRIPT", stub):
            rc, lines, records = self.run_check()
        self.assertEqual(rc, 1)
        self.assertIn("smbios --format text: check failed (exit 1)", lines[0])
        self.assertNothingLeaked("\n".join(lines), records)

    def test_KnownFailReasonIsShown(self) -> None:
        self.install_cli()
        stub = self.checker("sys.stderr.write('FAIL: invalid JSON at line 3, column 9\\n')\n"
                            "sys.exit(1)\n")
        with mock.patch.object(cfc, "CHECK_SCRIPT", stub):
            rc, lines, _ = self.run_check(backend="plain")
        self.assertEqual(rc, 1)
        self.assertIn("invalid JSON at line 3, column 9", lines[0])


class HelperTests(unittest.TestCase):
    """find_cli, sysfs_info, the regexes and sudo argv (no child processes)."""

    def test_FindCliMatchesOnlyTheCliPath(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            self.assertIsNone(cfc.find_cli(tmp / "missing"))
            (tmp / "tests" / "bin").mkdir(parents=True)
            (tmp / "tests" / "bin" / "smbios").write_text("x")
            self.assertIsNone(cfc.find_cli(tmp))
            target = tmp / "smbios" / "bin"
            target.mkdir(parents=True)
            if os.name != "nt":
                (target / "smbios").symlink_to(tmp / "tests" / "bin" / "smbios")
                self.assertIsNone(cfc.find_cli(tmp), "a symlink is never returned")
                (target / "smbios").unlink()
            (target / "smbios.exe").write_text("x")
            self.assertEqual(cfc.find_cli(tmp), target / "smbios.exe")

    @unittest.skipIf(os.name == "nt", "POSIX modes")
    def test_SysfsInfoListsAtMostThreeTriples(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            for name in ("DMI", "smbios_entry_point", "zz", "zzz", "::error::x"):
                (tmp / name).write_text("TEST-SERIAL-0000")
                os.chmod(tmp / name, 0o400)
            line = cfc.sysfs_info(tmp)
            self.assertTrue(line.startswith("info: sysfs dmi tables: ? 0400 "), line)
            self.assertEqual(line.count("0400"), 3)
            self.assertNotIn("::error", line)
            self.assertNotIn(SERIAL, line)
            self.assertIn("unavailable", cfc.sysfs_info(tmp / "missing"))

    def test_OkRegexMatchesTheCheckerWording(self) -> None:
        self.assertIsNotNone(cfc.OK_COUNT_LINE.match("ok: csv, 239 rows"))
        self.assertIsNotNone(cfc.OK_COUNT_LINE.match("ok: json, 26 structures"))
        self.assertIsNone(cfc.OK_COUNT_LINE.match("ok: json, 26 structuresX"))
        self.assertIsNone(cfc.OK_COUNT_LINE.match(" ok: json, 26 structures"))
        self.assertIsNotNone(cfc.OK_TEXT_LINE.match("ok: text, non-empty"))
        source = cfc.CHECK_SCRIPT.read_text(encoding="utf-8")
        self.assertIn('print("ok: text, non-empty")', source)
        self.assertIn('print(f"ok: {args.format}, {count} {unit}")', source)

    def test_FailReasonsCoverTheCheckerMessages(self) -> None:
        for reason in ("command exited with status 1", "stdout is empty",
                       "JSON root is not an array", 'JSON element 2 has no string "type"',
                       "XML contains a <!DOCTYPE declaration, which is not allowed",
                       "CSV row 4 has 2 columns, expected 3",
                       "YAML line 5 is neither a '- type:' item nor indented under one"):
            with self.subTest(reason=reason):
                self.assertTrue(any(rx.match(reason) for rx in cfc.FAIL_REASONS))
        self.assertFalse(any(rx.match("stdout is empty TEST-SERIAL-0000")
                             for rx in cfc.FAIL_REASONS))

    def test_SudoPrecedesTheCli(self) -> None:
        seen: List[List[str]] = []
        with tempfile.TemporaryDirectory() as tmp_name:
            log = Path(tmp_name) / "x.log"
            log.write_text("ok: json, 2 structures\n")

            def fake(label, argv, ctx):
                seen.append(list(argv))
                return 0, log
            ctx = cfc._Ctx(get_reporter("plain"), Path(tmp_name), Path(tmp_name) / "r.jsonl")
            with mock.patch.object(cfc, "_run_child", side_effect=fake):
                self.assertEqual(cfc.check_format("py", Path("/b/smbios"), "json", True,
                                                  ctx=ctx), (True, 2, "structures"))
                cfc.check_format("py", Path("/b/smbios"), "json", False, ctx=ctx)
        cli = str(Path("/b/smbios"))
        self.assertEqual(seen[0][seen[0].index("--") + 1:], ["sudo", "-n", cli, "--format",
                                                             "json"])
        self.assertEqual(seen[1][seen[1].index("--") + 1:], [cli, "--format", "json"])

    def test_UnknownFormatIsRejected(self) -> None:
        with self.assertRaises(ValueError):
            cfc.check_format("py", Path("x"), "html", False,
                             ctx=cfc._Ctx(get_reporter("plain"), Path("."), Path("r")))


class ReporterWarningTests(unittest.TestCase):
    """The standalone ``warning()`` event on every backend (CR-4)."""

    def test_EveryBackendRendersOneSafeLine(self) -> None:
        hostile = "x\n::error::y ##teamcity[buildStatus text='z']"
        github = get_reporter("github").warning("empty table", hostile)
        teamcity = get_reporter("teamcity").warning("empty table", hostile)
        plain = get_reporter("plain").warning("empty table", hostile)
        for lines in (github, teamcity, plain):
            self.assertEqual(len(lines), 1)
        self.assertTrue(github[0].startswith("::warning title=empty table::x"))
        self.assertNotIn("\n", github[0])
        self.assertTrue(teamcity[0].startswith("##teamcity[message text='empty table: x"))
        self.assertEqual(teamcity[0].count("##teamcity["), 1)
        self.assertTrue(teamcity[0].endswith("status='WARNING']"))
        self.assertEqual(plain, ["warning: empty table: x"])

    def test_GithubOutputAndSummaryFallBackToTheRunnerFiles(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            out, summary = tmp / "out", tmp / "summary.md"
            env = {"GITHUB_OUTPUT": str(out), "GITHUB_STEP_SUMMARY": str(summary)}
            with mock.patch.dict(os.environ, env):
                os.environ.pop("CI_OUTPUT_FILE", None)
                os.environ.pop("CI_SUMMARY_FILE", None)
                reporter = get_reporter("github")
                reporter.set_output("cache_enabled", True)
                reporter.job_summary("env: build/linux")
            self.assertIn("cache_enabled<<", out.read_text())
            self.assertEqual(summary.read_text(), "env: build/linux\n")
            neutral = tmp / "neutral"
            with mock.patch.dict(os.environ, {**env, "CI_OUTPUT_FILE": str(neutral)}):
                get_reporter("github").set_output("x", 1)
            self.assertTrue(neutral.exists(), "the neutral name wins over the fallback")
            self.assertNotIn("x<<", out.read_text())

    def test_GithubOutputWithoutAnyTargetExits2(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaises(SystemExit) as caught, \
                    contextlib.redirect_stderr(io.StringIO()):
                get_reporter("github").set_output("x", 1)
        self.assertEqual(caught.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
