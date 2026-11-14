"""Tests for the step-runner security follow-ups (milestone 0014, Story 02.0 Task 04).

Covers the threat-model per-task requirements 1-5 (docs/security/2026-11-13-ci-rework.md):

1. CR-5 child-environment scrub - ``test_ci_step.py``'s
   ``test_ChildDoesNotInheritDebugSwitches`` / ``test_PassEnvKeepsOnlyTheNamedDeniedName``.
2. CR-4 unfenced lines - :class:`UnfencedLineTests` (every backend, every
   unfenced renderer, plus the TeamCity ``buildProblem`` first diagnostic).
3. CR-9 ``--hygiene counts-only`` - :class:`CountsOnlyHygieneTests`.
4. CR-11 file safety - :class:`FileSafetyTests`.
5. CR-4 ``hostile.log`` legacy forms - :class:`HostileFixtureTests`.

Plus the CR-6/CR-8 job-output channel (:class:`SetOutputTests`), the new
neutral names (:class:`EnvNameAdditionsTests`) and the P11 single-emitter rule
over the whole ``scripts/ci`` tree (:class:`SingleEmitterTests`).

Classification: unit (reporter rendering, ``child_environment``) and
process-level integration (``ci_step.py run`` as a subprocess). Stdlib only,
no network, synthetic logs only; ``TEST-SERIAL-0000`` is a fake identifier.
On failure first check: ``reporters/__init__.py`` (``_clean``,
``output_json``), ``ci_step.counts_only_line`` and ``ci_step.prepare_log_dir``.
"""

from __future__ import annotations

import contextlib
import io
import json
import os
import stat
import sys
import tempfile
import unittest
from pathlib import Path
from typing import List
from unittest import mock

from ci_test_support import CI_DIR, FIXTURES, cat_argv, neutral_env, run_cli

import check_workflow_policy as gate  # noqa: E402
import ci_step  # noqa: E402
import env_names  # noqa: E402
import reporters  # noqa: E402
from reporters.plain import PlainReporter  # noqa: E402

BACKENDS = ("github", "teamcity", "plain")
#: Built by concatenation so this file holds no literal P11 needle.
TC = "##" + "teamcity["
SERIAL = "TEST-SERIAL-0000"

#: Hostile summaries for every unfenced renderer (CR-4).
HOSTILE = (
    f"x {TC}buildStatus text='green']",
    "::" + "error::forged",
    "##[error]forged",
    "##[set-output name=x;]y",
    f"{TC}enableServiceMessages]",
    "first\n::" + "warning::second line",
    f"\x1b[31mred\x1b[0m {TC}message text='x']",
    "\r::" + "error::after CR",
)


class _TmpCase(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="ci-sec-test-")
        self.tmp = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)


class UnfencedLineTests(_TmpCase):
    """CR-4: ``step_ok``/``step_skip``/``info`` never print an interpretable line."""

    def assert_inert(self, lines: List[str]) -> None:
        for line in lines:
            self.assertNotIn(TC, line, line)
            self.assertFalse(line.startswith("::"), line)
            self.assertFalse(line.lstrip().startswith("##["), line)
            self.assertNotRegex(line, r"[\x00-\x08\x0a-\x1f\x7f]", line)

    def test_HostileSummaryInEveryBackend(self) -> None:
        for backend in BACKENDS:
            rep = reporters.get_reporter(backend)
            for text in HOSTILE:
                with self.subTest(backend=backend, text=text):
                    ok = rep.step_ok("s", text, 1.0)
                    skip = rep.step_skip("s", text)
                    info = rep.info(text)
                    for rendered in (ok, skip, info):
                        self.assertEqual(len(rendered), 1)
                        self.assert_inert(rendered)
                    self.assertTrue(ok[0].startswith("ok: s"), ok)
                    self.assertTrue(skip[0].startswith("skip: s: "), skip)

    def test_HostileLabelIsSanitisedToo(self) -> None:
        for backend in BACKENDS:
            with self.subTest(backend=backend):
                rep = reporters.get_reporter(backend)
                self.assert_inert(rep.step_ok(f"a {TC}x]\n::" + "error::y", "", 0.0))
                self.assert_inert(rep.step_skip(f"a {TC}x]", "r"))

    def test_TeamcityAuthorsTheOnlyServiceMessage(self) -> None:
        rep = reporters.get_reporter("teamcity")
        head = rep.step_fail("s", 1, 0.0, f"diag {TC}buildStatus text='green']", ["x"])[0]
        self.assertEqual(head.count(TC), 1, head)
        self.assertTrue(head.startswith(TC + "buildProblem "), head)
        warn = rep.step_warn("s", f"why {TC}x]", 1, [])[0]
        self.assertEqual(warn.count(TC), 1, warn)

    def test_SafeOkLineThroughTheRunner(self) -> None:
        """End to end: a hostile ``--summary`` on the TeamCity backend."""
        r = run_cli(self.tmp, ["run", "--reporter", "teamcity", "--label", "s", "--summary",
                               f"3 ok {TC}buildStatus text='green']", "--",
                               sys.executable, "-c", "pass"])
        self.assertEqual((r.rc, len(r.lines)), (0, 1), r.lines)
        self.assert_inert(r.lines)


class HostileFixtureTests(_TmpCase):
    """CR-4 item 5: the legacy ``##[...]`` forms and a mid-line service message stay fenced."""

    def test_LegacyFormsAreInTheFixtureAndFenced(self) -> None:
        text = (FIXTURES / "hostile.log").read_text(encoding="utf-8", errors="replace")
        for needle in ("##[set-output name=x;]y", "##[error]x",
                       f"ok: x {TC}buildStatus text='green']"):
            self.assertIn(needle, text)
        for backend in BACKENDS:
            with self.subTest(backend=backend):
                env = neutral_env(self.tmp, CI_RESULTS_FILE=str(self.tmp / f"{backend}.jsonl"))
                r = run_cli(self.tmp, ["run", "--reporter", backend, "--label", "hostile", "--",
                                       *cat_argv(FIXTURES / "hostile.log", 1)], env=env)
                self.assertEqual(r.rc, 1)
                if backend == "teamcity":
                    authored = [ln for ln in r.lines if TC in ln]
                    self.assertEqual(len(authored), 3, authored)  # problem + fence pair
                    self.assertTrue(r.lines[1].endswith("disableServiceMessages]"))
                if backend == "github":
                    token = r.lines[1].split("::")[2]
                    end = r.lines.index(f"::{token}::")
                    outside = r.lines[:2] + r.lines[end:]
                    self.assertFalse(any(ln.startswith("##[") for ln in outside))


#: A CTest run with a fake firmware serial in every non-status place.
_CTEST_LOG = f"""Test project /build/linux
    Start 1: unit
1/3 Test #1: unit .............................   Passed    0.10 sec
    Start 2: integration
System serial number: {SERIAL}
src/x.cpp:1:1: error: table says {SERIAL}
2/3 Test #2: integration ......................***Failed    0.20 sec {SERIAL}
    Start 3: perf
3/3 Test #3: perf .............................   Passed    0.30 sec

67% tests passed, 1 tests failed out of 3

Total Test time (real) =   0.61 sec

The following tests FAILED:
\t  2 - integration (Failed) {SERIAL}
Errors while running CTest
{SERIAL}
"""


class CountsOnlyHygieneTests(_TmpCase):
    """CR-9 / RC1 data hygiene: ``--hygiene counts-only`` shows CTest status lines only."""

    def _run(self, backend: str, log_text: str, rc: int = 8):
        log = self.tmp / "ctest-input.log"
        log.write_text(log_text, encoding="utf-8")
        env = neutral_env(self.tmp, CI_RESULTS_FILE=str(self.tmp / f"{backend}.jsonl"))
        return run_cli(self.tmp, ["run", "--reporter", backend, "--label", "live table",
                                  "--patterns", "ctest,cxx", "--hygiene", "counts-only", "--",
                                  *cat_argv(log, rc)], env=env)

    def test_FakeSerialNeverLeaves(self) -> None:
        for backend in BACKENDS:
            with self.subTest(backend=backend):
                r = self._run(backend, _CTEST_LOG)
                self.assertEqual(r.rc, 8)
                out = "\n".join(r.lines)
                self.assertNotIn(SERIAL, out)
                self.assertNotIn(SERIAL, r.stderr)
                records = (self.tmp / f"{backend}.jsonl").read_text(encoding="ascii")
                self.assertNotIn(SERIAL, records)
                self.assertIn("1/3 Test #1: unit ......", out)
                self.assertIn("2/3 Test #2: integration ......................***Failed    "
                              "0.20 sec", out)
                self.assertIn("67% tests passed, 1 tests failed out of 3", out)
                self.assertIn("Total Test time (real) =   0.61 sec", out)
                self.assertNotIn("Start 2", out)
                self.assertNotIn("Errors while running CTest", out)
                rec = r.records[-1]
                self.assertIn("***Failed", rec["first_diagnostic"])
                self.assertNotIn(SERIAL, json.dumps(rec))
                # The full log is kept, under live/ only.
                live = self.tmp / "logs" / "live" / "live_table.log"
                self.assertIn(SERIAL, live.read_text(encoding="utf-8"))
                self.assertFalse((self.tmp / "logs" / "live_table.log").exists())

    def test_NoStatusLinesMeansSeeStatusLines(self) -> None:
        r = self._run("github", f"Segmentation fault while reading {SERIAL}\n", rc=139)
        self.assertEqual(r.rc, 139)
        self.assertNotIn(SERIAL, "\n".join(r.lines))
        self.assertEqual(r.records[-1]["first_diagnostic"], ci_step.SEE_STATUS_LINES)
        self.assertEqual(len(r.lines), 1, "nothing from the log may be shown")

    def test_StatusLineRegexIsAnchored(self) -> None:
        self.assertIsNone(ci_step.counts_only_line(f"{SERIAL} 1/3 Test #1: a ... Passed"))
        self.assertIsNone(ci_step.counts_only_line(f"{SERIAL} 67% tests passed, 0 tests "
                                                   "failed out of 3"))
        self.assertIsNone(ci_step.counts_only_line(f"Total Test time (real) = 1 sec {SERIAL}"))
        self.assertEqual(ci_step.counts_only_line(
            f"1/1 Test #1: a ...***Exception: SegFault  0.01 sec {SERIAL}"),
            "1/1 Test #1: a ...***Exception")

    def test_FullModeIsUnchangedByDefault(self) -> None:
        r = run_cli(self.tmp, ["run", "--label", "plainlog", "--", sys.executable, "-c",
                               f"print('{SERIAL}'); raise SystemExit(1)"])
        self.assertIn(SERIAL, "\n".join(r.lines), "full mode keeps today's excerpt")
        self.assertTrue((self.tmp / "logs" / "plainlog.log").exists())

    def test_UnknownHygieneIsUsageError(self) -> None:
        r = run_cli(self.tmp, ["run", "--label", "x", "--hygiene", "some", "--",
                               sys.executable, "-c", "pass"])
        self.assertEqual((r.rc, r.records), (2, []))
        with self.assertRaises(ValueError):
            ci_step.run_step("x", [sys.executable, "-c", "pass"], patterns=["ctest"],
                             best_effort_reason=None, summary=None, log_dir=self.tmp / "l",
                             reporter=PlainReporter(), results_file=self.tmp / "r.jsonl",
                             hygiene="some")  # type: ignore[arg-type]


class SetOutputTests(_TmpCase):
    """CR-6/CR-8: ``Reporter.set_output`` writes exactly one single-line JSON entry."""

    VALUES = ("a\nb", "c\rd", "EOF", "ci_output_" + "0" * 32, "ghadelimiter_x\nz<<EOF",
              {"matrix": ["linux-x64", "x\ny=1"], "n": 3})

    @staticmethod
    def parse_entries(text: str) -> List[tuple]:
        """Parse GitHub's ``name<<DELIM`` heredoc entries strictly."""
        lines, entries, i = text.split("\n"), [], 0
        while i < len(lines) and lines[i]:
            name, delim = lines[i].split("<<", 1)
            body: List[str] = []
            i += 1
            while lines[i] != delim:
                body.append(lines[i])
                i += 1
            entries.append((name, "\n".join(body), delim))
            i += 1
        return entries

    def test_GithubOneEntryPerCall(self) -> None:
        out_file = self.tmp / "out"
        env = neutral_env(self.tmp, CI_OUTPUT_FILE=str(out_file))
        with mock.patch.dict(os.environ, env, clear=True):
            rep = reporters.get_reporter("github")
            for value in self.VALUES:
                rep.set_output("digest", value)
        entries = self.parse_entries(out_file.read_text(encoding="ascii"))
        self.assertEqual(len(entries), len(self.VALUES))
        for (name, body, delim), value in zip(entries, self.VALUES):
            self.assertEqual(name, "digest")
            self.assertNotIn("\n", body)
            self.assertEqual(json.loads(body), value)
            self.assertRegex(delim, r"^ci_output_[0-9a-f]{32}$")
        self.assertEqual(len({d for _, _, d in entries}), len(entries), "delimiter reused")
        if os.name == "posix":
            self.assertEqual(stat.S_IMODE(out_file.stat().st_mode), 0o600)

    def test_GithubWithoutOutputFileExits2(self) -> None:
        with mock.patch.dict(os.environ, neutral_env(self.tmp), clear=True), \
                contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as cm:
                reporters.get_reporter("github").set_output("x", 1)
        self.assertEqual(cm.exception.code, 2)

    def test_TeamcityAndPlainPrintOneLine(self) -> None:
        for backend in ("teamcity", "plain"):
            for value in self.VALUES:
                with self.subTest(backend=backend, value=value):
                    out = io.StringIO()
                    with contextlib.redirect_stdout(out):
                        reporters.get_reporter(backend).set_output("plan", value)
                    printed = out.getvalue()
                    self.assertTrue(printed.endswith("\n"))
                    self.assertEqual(printed.count("\n"), 1, printed)
                    self.assertNotIn("\r", printed)
                    if backend == "teamcity":
                        self.assertEqual(printed.count(TC), 1)
                        self.assertTrue(printed.startswith(
                            TC + "setParameter name='plan' value='"), printed)
                        self.assertTrue(printed.endswith("']\n"))
                    else:
                        self.assertEqual(json.loads(printed.split("=", 1)[1]), value)

    def test_InvalidNameRaises(self) -> None:
        out_file = self.tmp / "out"
        env = neutral_env(self.tmp, CI_OUTPUT_FILE=str(out_file))
        bad = ("", "Digest", "1x", "x-y", "x y", "a" * 65, "x\ny", "x\n", "x<<EOF", "é")
        with mock.patch.dict(os.environ, env, clear=True):
            for backend in BACKENDS:
                rep = reporters.get_reporter(backend)
                for name in bad:
                    with self.subTest(backend=backend, name=name), \
                            contextlib.redirect_stdout(io.StringIO()) as out:
                        with self.assertRaises(ValueError):
                            rep.set_output(name, "v")
                        self.assertEqual(out.getvalue(), "")
                with self.assertRaises(ValueError):
                    rep.set_output("x", object())
        self.assertFalse(out_file.exists(), "nothing written for an invalid call")
        self.assertIsNotNone(reporters.OUTPUT_NAME.fullmatch("a" * 64))


@unittest.skipUnless(os.name == "posix", "POSIX modes and O_NOFOLLOW")
class FileSafetyTests(_TmpCase):
    """CR-11: private files, no symlink following, unsafe log directory refused."""

    def test_SymlinkedLogIsRefusedAndTargetUnchanged(self) -> None:
        logs = self.tmp / "logs"
        logs.mkdir(mode=0o700)
        target = self.tmp / "precious.txt"
        target.write_text("keep me\n", encoding="utf-8")
        (logs / "victim.log").symlink_to(target)
        marker = self.tmp / "ran"
        r = run_cli(self.tmp, ["run", "--label", "victim", "--", sys.executable, "-c",
                               f"open({str(marker)!r}, 'w').close()"])
        self.assertEqual(r.rc, 2)
        self.assertEqual(target.read_text(encoding="utf-8"), "keep me\n")
        self.assertFalse(marker.exists(), "the step must not run")
        self.assertEqual(r.records, [])
        self.assertNotIn("Traceback", r.stderr)
        with self.assertRaises(ci_step.UnsafePathError), \
                contextlib.redirect_stdout(io.StringIO()):
            ci_step.run_step("victim", [sys.executable, "-c", "pass"], patterns=["python"],
                             best_effort_reason=None, summary=None, log_dir=logs,
                             reporter=PlainReporter(), results_file=self.tmp / "r.jsonl")
        self.assertEqual(target.read_text(encoding="utf-8"), "keep me\n")

    def test_CreatedFilesArePrivate(self) -> None:
        old = os.umask(0o022)
        self.addCleanup(os.umask, old)
        for hygiene in ("full", "counts-only"):
            with self.subTest(hygiene=hygiene):
                r = run_cli(self.tmp, ["run", "--label", f"m-{hygiene}", "--hygiene", hygiene,
                                       "--", sys.executable, "-c", "pass"])
                self.assertEqual(r.rc, 0)
        logs = self.tmp / "logs"
        self.assertEqual(stat.S_IMODE(logs.stat().st_mode), 0o700)
        self.assertEqual(stat.S_IMODE((logs / "live").stat().st_mode), 0o700)
        self.assertEqual(stat.S_IMODE((logs / "m-full.log").stat().st_mode), 0o600)
        self.assertEqual(stat.S_IMODE((logs / "live" / "m-counts-only.log").stat().st_mode),
                         0o600)
        self.assertEqual(stat.S_IMODE((self.tmp / "results.jsonl").stat().st_mode), 0o600)
        # An existing, wider log is narrowed when rewritten.
        wide = logs / "m-full.log"
        wide.chmod(0o644)
        run_cli(self.tmp, ["run", "--label", "m-full", "--", sys.executable, "-c", "pass"])
        self.assertEqual(stat.S_IMODE(wide.stat().st_mode), 0o600)

    def test_ChildKeepsCallerUmask(self) -> None:
        """The umask is scoped to the runner's own files, not inherited by the child."""
        old = os.umask(0o022)
        self.addCleanup(os.umask, old)
        made = self.tmp / "child-made"
        r = run_cli(self.tmp, ["run", "--label", "umask", "--", sys.executable, "-c",
                               f"open({str(made)!r}, 'w').close()"])
        self.assertEqual(r.rc, 0)
        self.assertEqual(stat.S_IMODE(made.stat().st_mode), 0o644)

    def test_WritableLogDirNotCreatedByRunnerExits2(self) -> None:
        for mode in (0o777, 0o770, 0o1777):
            with self.subTest(mode=oct(mode)):
                shared = self.tmp / f"shared-{mode:o}"
                shared.mkdir()
                shared.chmod(mode)
                env = neutral_env(self.tmp, CI_LOG_DIR=str(shared))
                r = run_cli(self.tmp, ["run", "--label", "x", "--", sys.executable, "-c", "pass"],
                            env=env)
                self.assertEqual(r.rc, 2)
                self.assertEqual(r.records, [])
                self.assertEqual(list(shared.iterdir()), [])
                self.assertEqual(len(r.stderr.splitlines()), 1, r.stderr)

    def test_LocalPlainFailurePrintsWhereLogsAre(self) -> None:
        r = run_cli(self.tmp, ["run", "--label", "f", "--", sys.executable, "-c",
                               "raise SystemExit(3)"])
        self.assertEqual(r.rc, 3)
        notes = [ln for ln in r.lines if ln.startswith("info: logs kept in ")]
        self.assertEqual(len(notes), 1, r.lines)
        self.assertEqual(r.lines[-1], notes[0])
        for backend in ("github", "teamcity"):
            with self.subTest(backend=backend):
                r = run_cli(self.tmp, ["run", "--reporter", backend, "--label", "f", "--",
                                       sys.executable, "-c", "raise SystemExit(3)"])
                self.assertFalse(any("logs kept in" in ln for ln in r.lines))
        r = run_cli(self.tmp, ["run", "--label", "f", "--", sys.executable, "-c",
                               "raise SystemExit(3)"],
                    env=neutral_env(self.tmp, CI_PLATFORM="github"))
        self.assertFalse(any("logs kept in" in ln for ln in r.lines), "local runs only")


class EnvNameAdditionsTests(unittest.TestCase):
    """New neutral names (ADR 0005 table, amended 2026-11-13)."""

    def test_RunAttemptAndOutputFile(self) -> None:
        self.assertIn("CI_RUN_ATTEMPT", env_names.NAMES)
        self.assertIn("CI_OUTPUT_FILE", env_names.NAMES)
        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(env_names.get("CI_RUN_ATTEMPT"), "1")
            self.assertEqual(env_names.get("CI_OUTPUT_FILE"), "")
        with mock.patch.dict(os.environ, {"CI_RUN_ATTEMPT": "3"}, clear=True):
            self.assertEqual(env_names.get("CI_RUN_ATTEMPT"), "3")

    def test_TeamcitySecureNamesAreDenied(self) -> None:
        for name in env_names.TEAMCITY_SECURE_NAMES:
            self.assertTrue(reporters.is_denied_child_name(name), name)
        for prefix in env_names.TEAMCITY_SECURE_PREFIXES:
            self.assertTrue(reporters.is_denied_child_name(prefix + "X"), prefix)
        self.assertFalse(reporters.is_denied_child_name("TEAMCITY" + "_VERSION"))


class SingleEmitterTests(unittest.TestCase):
    """P11 single-emitter rule over the whole tree (``lint.py``'s policy step)."""

    def test_WholeScriptsCiTreePassesP11(self) -> None:
        found = [v.render() for v in gate.check_scripts(CI_DIR)]
        self.assertEqual(found, [])

    def test_EmitErrorGoesToStderrThroughReporter(self) -> None:
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            ci_step.emit_error("x.py: error: ::" + "error::bad\nsecond")
        self.assertEqual(out.getvalue(), "")
        self.assertEqual(err.getvalue(), "x.py: error: ::" + "error::bad\n")


if __name__ == "__main__":
    unittest.main()
