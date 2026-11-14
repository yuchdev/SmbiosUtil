"""Tests for ``stage.py`` (milestone 0014, Story 03.0 Task 01; plan SC1/SC2, threat model CR-6/CR-8/CR-9).

Classification: unit tests (pure functions: tree digest, input validation,
tier selection) plus command-line tests of ``stage.py`` in a subprocess and
step tests against stub ``cmake``/``ctest`` commands on a temporary ``PATH``.
No network, no host cmake/ctest, no firmware data.

On failure, first check: ``scripts/ci/legs.json`` (legs, tiers, stages, each
leg's ``job``) and ``.github/workflows/stage-build.yml`` /
``stage-sanitizers.yml`` / ``stage-gui.yml`` (matrix expressions, runner
labels) - the tier and
runner tests compare each job's workflow with its legs, so an edit to one
without the other fails here.
"""

from __future__ import annotations

import contextlib
import io
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Dict, List
from unittest import mock

from ci_test_support import CI_DIR, neutral_env, path_with, write_stub

import check_workflow_policy as gate  # noqa: E402
import stage  # noqa: E402
from reporters import get_reporter  # noqa: E402

REPO_ROOT = CI_DIR.parents[1]
STAGE = CI_DIR / "stage.py"
WORKFLOWS = REPO_ROOT / ".github" / "workflows"
#: Reusable stage workflow of each leg job (legs.json "job").
JOB_WORKFLOWS = {"build": "stage-build.yml", "sanitizers": "stage-sanitizers.yml",
                 "gui": "stage-gui.yml"}
STAGE_WORKFLOWS = ("stage-lint.yml", "stage-build.yml", "stage-sanitizers.yml", "stage-gui.yml")
POSIX_ONLY = "stub commands are /bin/sh trampolines (POSIX only)"


def run_stage_cli(tmp: Path, args: List[str], **extra: str) -> subprocess.CompletedProcess:
    env = neutral_env(tmp, **{"CI_WORKSPACE": str(REPO_ROOT), "CI_PLATFORM": "local", **extra})
    return subprocess.run([sys.executable, "-I", str(STAGE), *args], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, stdin=subprocess.DEVNULL, env=env,
                          cwd=str(tmp), timeout=120, text=True)


def make_ctx(tmp: Path, leg: stage.Leg, *, reporter: str = "plain", event: str = "",
             ref: str = "", platform: str = "local", stage_name: str = "build") -> stage.Context:
    return stage.Context(stage=stage_name, leg=leg, reporter=get_reporter(reporter),
                         workspace=tmp / "ws", temp_dir=tmp, log_dir=tmp / "logs",
                         results=tmp / "results.jsonl", artifact_dir=tmp / "artifacts",
                         event=event, ref=ref, platform=platform)


def records(tmp: Path) -> List[dict]:
    path = tmp / "results.jsonl"
    if not path.exists():
        return []
    return [json.loads(x) for x in path.read_text(encoding="ascii").splitlines()]


class TreeDigestTests(unittest.TestCase):
    """SC2/CR-8/CR-9: the install-tree digest and what it refuses."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name) / "install" / "linux-x64"
        (self.root / "lib").mkdir(parents=True)
        (self.root / "include" / "smbios").mkdir(parents=True)
        (self.root / "lib" / "libsmbios_c.so").write_bytes(b"\x7fELF fake")
        (self.root / "include" / "smbios" / "smbios.h").write_text("int x;\n", encoding="utf-8")

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def test_DigestIsStableAndCountsFiles(self) -> None:
        first = stage.tree_digest(self.root)
        self.assertEqual(first, stage.tree_digest(self.root))
        self.assertEqual(first[1], 2)
        self.assertRegex(first[0], r"^[0-9a-f]{64}$")

    def test_DigestFormatIsPathNulFileHash(self) -> None:
        import hashlib
        expected = hashlib.sha256()
        for rel in ("include/smbios/smbios.h", "lib/libsmbios_c.so"):
            data = (self.root / rel).read_bytes()
            expected.update(rel.encode() + b"\0" + hashlib.sha256(data).hexdigest().encode()
                            + b"\n")
        self.assertEqual(stage.tree_digest(self.root)[0], expected.hexdigest())

    def test_AddedFileChangesDigest(self) -> None:
        before = stage.tree_digest(self.root)[0]
        (self.root / "lib" / "extra.a").write_bytes(b"x")
        self.assertNotEqual(before, stage.tree_digest(self.root)[0])

    def test_ChangedFileChangesDigest(self) -> None:
        before = stage.tree_digest(self.root)[0]
        (self.root / "include" / "smbios" / "smbios.h").write_text("int y;\n", encoding="utf-8")
        self.assertNotEqual(before, stage.tree_digest(self.root)[0])

    def test_RenamedFileChangesDigest(self) -> None:
        before = stage.tree_digest(self.root)[0]
        (self.root / "lib" / "libsmbios_c.so").rename(self.root / "lib" / "libsmbios_d.so")
        self.assertNotEqual(before, stage.tree_digest(self.root)[0])

    @unittest.skipIf(os.name == "nt", "symlink creation needs privileges on Windows")
    def test_SymlinkedFileFails(self) -> None:
        (self.root / "lib" / "link.so").symlink_to(self.root / "lib" / "libsmbios_c.so")
        with self.assertRaisesRegex(stage.DigestError, "symlink: lib/link.so"):
            stage.tree_digest(self.root)

    @unittest.skipIf(os.name == "nt", "symlink creation needs privileges on Windows")
    def test_SymlinkedDirectoryFails(self) -> None:
        outside = Path(self._tmp.name) / "outside"
        outside.mkdir()
        (self.root / "share").symlink_to(outside, target_is_directory=True)
        with self.assertRaisesRegex(stage.DigestError, "symlink"):
            stage.tree_digest(self.root)

    @unittest.skipIf(os.name == "nt", "symlink creation needs privileges on Windows")
    def test_SymlinkedRootFails(self) -> None:
        link = Path(self._tmp.name) / "rootlink"
        link.symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(stage.DigestError, "not a directory"):
            stage.tree_digest(link)

    def test_TestingDirectoryFails(self) -> None:
        (self.root / "share" / "Testing" / "Temporary").mkdir(parents=True)
        (self.root / "share" / "Testing" / "Temporary" / "LastTest.log").write_text(
            "TEST-SERIAL-0000\n", encoding="utf-8")
        with self.assertRaisesRegex(stage.DigestError, "Testing path"):
            stage.tree_digest(self.root)

    def test_TestingFileNameFails(self) -> None:
        (self.root / "Testing").write_text("x", encoding="utf-8")
        with self.assertRaisesRegex(stage.DigestError, "Testing path"):
            stage.tree_digest(self.root)

    @unittest.skipUnless(hasattr(os, "mkfifo"), "needs mkfifo")
    def test_NonRegularFileFails(self) -> None:
        os.mkfifo(self.root / "lib" / "pipe")
        with self.assertRaisesRegex(stage.DigestError, "non-regular"):
            stage.tree_digest(self.root)

    def test_EmptyOrMissingTreeFails(self) -> None:
        empty = Path(self._tmp.name) / "empty"
        empty.mkdir()
        (empty / "dir").mkdir()
        with self.assertRaisesRegex(stage.DigestError, "empty"):
            stage.tree_digest(empty)
        with self.assertRaisesRegex(stage.DigestError, "missing"):
            stage.tree_digest(Path(self._tmp.name) / "nope")

    @unittest.skipIf(os.name == "nt", "':' is not allowed in Windows file names")
    def test_HostilePathIsNotEchoed(self) -> None:
        (self.root / "lib" / "::error::x").symlink_to(self.root / "lib" / "libsmbios_c.so")
        with self.assertRaises(stage.DigestError) as caught:
            stage.tree_digest(self.root)
        self.assertNotIn("::error", str(caught.exception))


class InputValidationTests(unittest.TestCase):
    """CR-6: --leg, --only, the stage and the neutral event inputs are re-validated (exit 2)."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def assertUsage(self, proc: subprocess.CompletedProcess, needle: str = "") -> None:
        self.assertEqual(proc.returncode, 2, proc.stdout + proc.stderr)
        self.assertEqual(proc.stdout, "")
        self.assertIn("stage.py: error:", proc.stderr)
        if needle:
            self.assertNotIn(needle, proc.stderr)

    def test_NewlineInLegExits2WithoutEcho(self) -> None:
        self.assertUsage(run_stage_cli(self.tmp, ["lint", "--leg", "linux\nx=y"]), "x=y")

    def test_UnknownOnlyExits2WithoutEcho(self) -> None:
        self.assertUsage(run_stage_cli(self.tmp, ["lint", "--only", "rm-rf"]), "rm-rf")

    def test_UnknownLegExits2(self) -> None:
        self.assertUsage(run_stage_cli(self.tmp, ["build", "--leg", "solaris"]), "solaris")

    def test_LegOfAnotherStageExits2(self) -> None:
        self.assertUsage(run_stage_cli(self.tmp, ["lint", "--leg", "windows"]))

    def test_PendingPackagingLegExits2(self) -> None:
        proc = run_stage_cli(self.tmp, ["build", "--leg", "linux-x64"])
        self.assertUsage(proc)
        self.assertIn("pending", proc.stderr)

    def test_UnknownAndUndefinedStagesExit2(self) -> None:
        self.assertUsage(run_stage_cli(self.tmp, ["deploy"]), "deploy")
        for name in ("package", "verify", "publish"):
            with self.subTest(stage=name):
                proc = run_stage_cli(self.tmp, [name])
                self.assertUsage(proc)
                self.assertIn("not defined yet", proc.stderr)

    def test_InvalidEventAndRefExit2(self) -> None:
        self.assertUsage(run_stage_cli(self.tmp, ["lint", "--only", "versions"],
                                       CI_EVENT="pull_request_target"), "pull_request_target")
        self.assertUsage(run_stage_cli(self.tmp, ["lint", "--only", "versions"],
                                       CI_EVENT="push", CI_REF="refs/heads/a\nb"))
        self.assertUsage(run_stage_cli(self.tmp, ["lint", "--only", "versions"],
                                       CI_EVENT="push", CI_REF_NAME="a b"))

    def test_MissingLegOnCiExits2(self) -> None:
        proc = run_stage_cli(self.tmp, ["build", "--only", "install"], CI_PLATFORM="github")
        self.assertUsage(proc)

    def test_LegFromEnvironmentIsValidated(self) -> None:
        self.assertUsage(run_stage_cli(self.tmp, ["build", "--only", "install"],
                                       CI_LEG="linux;x"), "linux;x")
        ok = run_stage_cli(self.tmp, ["build", "--only", "install"], CI_LEG="linux")
        self.assertEqual(ok.returncode, 0, ok.stderr)
        self.assertEqual(ok.stdout.splitlines(), ["skip: install tree: not a packaging leg"])

    def test_ValidatorsDirectly(self) -> None:
        legs = stage.load_legs()
        self.assertEqual(stage.validate_leg("build", "macos", legs).preset, "macos")
        self.assertEqual(stage.validate_only("test", "cli-formats"), "cli-formats")
        self.assertIsNone(stage.validate_only("test", None))
        for bad in ("", "LINUX", "-linux", "a" * 33, "linux\n"):
            with self.subTest(leg=bad), self.assertRaises(stage.UsageError):
                stage.validate_leg("build", bad, legs)
        stage.validate_event("tag", "refs/tags/v1.0.0", "v1.0.0")
        stage.validate_event("", "", "")


class TierTests(unittest.TestCase):
    """Plan Q1: Linux legs on a branch push; the full matrix on every other event."""

    legs = stage.load_legs()

    def test_TierTable(self) -> None:
        full = ("linux", "windows", "macos", "linux-asan", "macos-asan", "linux-gui",
                "windows-gui", "macos-gui")
        cases = {("push", "refs/heads/feature"): ("linux", "linux-asan", "linux-gui"),
                 ("push", "refs/heads/master"): full,
                 ("pr", "refs/pull/7/merge"): full,
                 ("tag", "refs/tags/v1.0.0"): full,
                 ("schedule", "refs/heads/master"): full,
                 ("manual", "refs/heads/feature"): full}
        for (event, ref), want in cases.items():
            with self.subTest(event=event, ref=ref):
                self.assertEqual(stage.tier_legs(event, ref, self.legs), want)
        self.assertIsNone(stage.tier_legs("", "", self.legs))
        self.assertTrue(stage.leg_selected("windows", "", "", self.legs))
        self.assertFalse(stage.leg_selected("windows", "push", "refs/heads/x", self.legs))
        self.assertFalse(stage.leg_selected("macos-asan", "push", "refs/heads/x", self.legs))
        self.assertTrue(stage.leg_selected("linux-asan", "push", "refs/heads/x", self.legs))
        self.assertTrue(stage.leg_selected("linux-gui", "push", "refs/heads/x", self.legs))
        self.assertFalse(stage.leg_selected("windows-gui", "push", "refs/heads/x", self.legs))

    def test_LegOutsideTierSkipsAndDisablesCache(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            out = tmp / "outputs"
            proc = run_stage_cli(tmp, ["build", "--leg", "macos", "--only", "prepare",
                                       "--reporter", "github"], CI_EVENT="push",
                                 CI_REF="refs/heads/feature", CI_OUTPUT_FILE=str(out))
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertEqual(proc.stdout.splitlines(),
                             ["skip: build macos: not in the push tier (Q1)"])
            text = out.read_text(encoding="ascii")
            self.assertIn("cache_enabled<<", text)
            self.assertIn("\nfalse\n", text)

    def test_LegOutsideTierOnCiIsAnError(self) -> None:
        """S3-5: on CI a scheduled leg the tier excludes is a mismatch (exit 2), never a skip."""
        for platform in ("github", "teamcity"):
            for args in (["test", "--leg", "windows-gui"], ["build", "--leg", "macos"]):
                with self.subTest(platform=platform, args=args), \
                        tempfile.TemporaryDirectory() as tmp_name:
                    tmp = Path(tmp_name)
                    proc = run_stage_cli(tmp, [*args, "--reporter", "plain"],
                                         CI_PLATFORM=platform, CI_EVENT="push",
                                         CI_REF="refs/heads/feature",
                                         CI_OUTPUT_FILE=str(tmp / "outputs"))
                    self.assertEqual(proc.returncode, stage.EXIT_USAGE,
                                     proc.stdout + proc.stderr)
                    self.assertNotIn("skip:", proc.stdout)
                    self.assertIn(f"leg {args[2]} is scheduled on CI but not in the push tier",
                                  proc.stderr)

    def test_LegInTierOnCiIsNotATierError(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            proc = run_stage_cli(tmp, ["test", "--leg", "linux-gui", "--only", "unit",
                                       "--reporter", "plain"], CI_PLATFORM="github",
                                 CI_EVENT="push", CI_REF="refs/heads/feature")
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertEqual(proc.stdout.splitlines(),
                             ["skip: test unit: not a step of the gui job"])

    def test_LocalTierSkipStillSkips(self) -> None:
        """The deliberate local tier simulation keeps its ``skip:`` line."""
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            proc = run_stage_cli(tmp, ["test", "--leg", "windows-gui", "--reporter", "plain"],
                                 CI_EVENT="push", CI_REF="refs/heads/feature")
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertEqual(proc.stdout.splitlines(),
                             ["skip: test windows-gui: not in the push tier (Q1)"])

    @staticmethod
    def _neutral_event(expr: str, github: Dict[str, str]) -> str:
        """Evaluate the workflow's ``a && 'x' || b && 'y' || github.event_name`` chain."""
        for part in [p.strip() for p in expr.split("||")]:
            m = re.fullmatch(r"(.+?) && '([a-z]+)'", part)
            if m is None:
                return github[part.split(".", 1)[1]]
            if gate._truth(gate._Evaluator(m.group(1), github).run()) is True:
                return m.group(2)
        raise AssertionError("no branch of the CI_EVENT chain applied")

    def test_WorkflowMatrixMatchesLegsJsonTiers(self) -> None:
        for job, workflow in JOB_WORKFLOWS.items():
            root = gate.load_yaml((WORKFLOWS / workflow).read_text(encoding="utf-8"))
            event_expr = gate._expressions(str(root["env"]["CI_EVENT"]))[0]
            leg_expr = gate._expressions(str(root["jobs"][job]["strategy"]["matrix"]["leg"]))[0]
            m = re.fullmatch(r"\s*fromJSON\((.+) && '(\[[^']*\])' \|\| '(\[[^']*\])'\)\s*",
                             leg_expr)
            self.assertIsNotNone(m, f"{workflow}: matrix leg expression changed shape")
            cond, narrow, wide = m.group(1), json.loads(m.group(2)), json.loads(m.group(3))
            for name, github in gate.SCENARIOS.items():
                with self.subTest(job=job, scenario=name):
                    narrowed = gate._truth(gate._Evaluator(cond, github).run())
                    self.assertIn(narrowed, (True, False))
                    workflow_legs = tuple(narrow if narrowed else wide)
                    event = self._neutral_event(event_expr, github)
                    self.assertIn(event, stage.EVENTS)
                    tier = stage.tier_legs(event, github["ref"], self.legs)
                    self.assertEqual(workflow_legs, tuple(
                        leg for leg in tier
                        if self.legs["legs"][leg].get("job", "build") == job))


class LegsDataTests(unittest.TestCase):
    """legs.json is consistent with itself, pins.json and the workflow."""

    legs = stage.load_legs()
    pins = json.loads((CI_DIR / "pins.json").read_text(encoding="utf-8"))

    def test_StagesAndTiersNameDeclaredLegs(self) -> None:
        declared = set(self.legs["legs"])
        for name, members in self.legs["stages"].items():
            self.assertLessEqual(set(members), declared, name)
        for name, members in self.legs["tiers"].items():
            self.assertLessEqual(set(members), set(self.legs["stages"]["build"]), name)

    def test_EveryLegParsesAndNamesAPinnedRunner(self) -> None:
        for name, entry in self.legs["legs"].items():
            with self.subTest(leg=name):
                leg = stage._leg(name, entry)
                self.assertIn(leg.runner, self.pins["runners"])
                self.assertRegex(name, stage._LEG_NAME.pattern)
                if leg.packaging:
                    self.assertRegex(name, r"^(linux|windows|macos)-(x64|arm64)$")

    def test_WorkflowRunnerLabelsArePinned(self) -> None:
        for leg in self.legs["tiers"]["full"]:
            job = self.legs["legs"][leg].get("job", "build")
            root = gate.load_yaml((WORKFLOWS / JOB_WORKFLOWS[job]).read_text(encoding="utf-8"))
            runs_on = gate._expressions(str(root["jobs"][job]["runs-on"]))[0]
            label = None
            for part in [p.strip() for p in runs_on.split("||")]:
                m = re.fullmatch(r"matrix\.leg == '([a-z-]+)' && '([^']+)'", part)
                if m is None:
                    label = part.strip("'")
                    break
                if m.group(1) == leg:
                    label = m.group(2)
                    break
            with self.subTest(leg=leg):
                runner = self.legs["legs"][leg]["runner"]
                self.assertEqual(label, self.pins["runners"][runner])

    def test_RealWorkflowsPassThePolicyGate(self) -> None:
        policy = gate.load_policy()
        for name in ("ci.yml",) + STAGE_WORKFLOWS:
            with self.subTest(workflow=name):
                found = gate.check_workflow(WORKFLOWS / name, gate.ALLOWED_WORKFLOW_FILES, policy)
                self.assertEqual([v.render() for v in found], [])

    def test_EveryRunLineIsOneStageCommand(self) -> None:
        for name in STAGE_WORKFLOWS:
            for body in gate.run_blocks(WORKFLOWS / name):
                with self.subTest(workflow=name, run=body):
                    self.assertRegex(body.strip(),
                                     r"^python3? scripts/ci/stage\.py (lint|build|test)"
                                     r"( --only [a-z-]+)?$")


class StepTests(unittest.TestCase):
    """Steps against stub cmake/ctest on a temporary PATH (no host tools, no network)."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        self.bin = self.tmp / "bin"
        (self.tmp / "ws").mkdir()
        self.argv_file = self.tmp / "argv.json"
        env = {"PATH": path_with(self.bin), "STUB_ARGV": str(self.argv_file),
               "CI_RESULTS_FILE": str(self.tmp / "results.jsonl"),
               "CI_LOG_DIR": str(self.tmp / "logs"), "CI_TEMP_DIR": str(self.tmp)}
        self._env = mock.patch.dict(os.environ, env)
        self._env.start()

    def tearDown(self) -> None:
        self._env.stop()
        self._tmp.cleanup()

    def stub(self, name: str, body: str) -> None:
        write_stub(self.bin, name, "import json, os, sys\n"
                   "with open(os.environ['STUB_ARGV'], 'a') as fh:\n"
                   "    fh.write(json.dumps(sys.argv[1:]) + '\\n')\n" + body)

    def argvs(self) -> List[List[str]]:
        return [json.loads(x) for x in self.argv_file.read_text().splitlines()]

    def capture(self, fn, *args) -> tuple:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = fn(*args)
        return rc, out.getvalue().splitlines()

    def test_SanitizerLegsAndQ4Data(self) -> None:
        data = stage.load_legs()
        legs = data["legs"]
        for name in ("linux-asan", "macos-asan"):
            with self.subTest(leg=name):
                leg = stage._leg(name, legs[name])
                self.assertTrue(leg.sanitizers and leg.unit_tests and not leg.live_table_sudo)
                self.assertIn(name, data["stages"]["build"])
                self.assertIn(name, data["stages"]["test"])
        self.assertFalse(legs["linux-asan"]["live_table_tests"], "Q4: skip under sanitizers")
        self.assertTrue(legs["macos-asan"]["live_table_tests"], "C14 kept on macOS")
        linux = stage._leg("linux", legs["linux"])
        self.assertTrue(linux.live_table_tests and linux.live_table_sudo, "Q4: sudo on linux")
        with self.assertRaises(stage.UsageError):
            stage._leg("x", {"runner": "linux", "preset": "linux", "config": "", "job": "bogus"})

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_UnitStepRunsVerbatimC06Selection(self) -> None:
        self.stub("ctest", "sys.exit(0)\n")
        leg = stage.Leg("windows", "windows", "windows", "Debug", unit_tests=True)
        rc, lines = self.capture(stage.step_unit, make_ctx(self.tmp, leg, stage_name="test"))
        self.assertEqual(rc, 0)
        self.assertEqual(len(lines), 1)
        self.assertTrue(lines[0].startswith("ok: tests unprivileged"))
        (argv,) = self.argvs()
        self.assertEqual(argv[argv.index("-C") + 1], "Debug")
        self.assertEqual(argv[argv.index("--timeout") + 1], "60")
        self.assertEqual(argv[argv.index("-R") + 1],
                         "smbios_unit_test|smbios_mock_test|smbios_c_|"
                         "smbios_cli_unknown_format|smbios_example_")
        self.assertIn("--output-on-failure", argv)

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_LiveTableStepIsBestEffortCountsOnly(self) -> None:
        self.stub("ctest", "print('1/2 Test #1: smbios_integration_test ....***Failed  0.1 sec')\n"
                           "print('Serial Number: TEST-SERIAL-0000')\n"
                           "sys.exit(8)\n")
        leg = stage.Leg("windows", "windows", "windows", "Debug", live_table_tests=True)
        rc, lines = self.capture(stage.step_live_table,
                                 make_ctx(self.tmp, leg, reporter="github", stage_name="test"))
        self.assertEqual(rc, 0, "best-effort never fails the stage (RC5)")
        text = "\n".join(lines)
        self.assertIn("::warning title=best-effort: tests live-table::", text)
        self.assertNotIn("TEST-SERIAL-0000", text)
        self.assertNotIn("TEST-SERIAL-0000", (self.tmp / "results.jsonl").read_text())
        self.assertTrue((self.tmp / "logs" / "live" / "tests_live-table.log").exists())
        (argv,) = self.argvs()
        self.assertNotIn("--output-on-failure", argv)
        self.assertEqual(argv[argv.index("-R") + 1],
                         "smbios_integration_test|smbios_perf_test|smbios_cli_format_csv")
        self.assertIn("--output-junit", argv)

    # --- Story 03.0 Task 02: sanitizer legs (C12-C14) and plan Q4 -------------

    ENV_PROBE = ("with open(os.environ['STUB_ARGV'] + '.env', 'a') as fh:\n"
                 "    fh.write(json.dumps([os.environ.get('ASAN_OPTIONS'),"
                 " os.environ.get('UBSAN_OPTIONS')]) + '\\n')\n")

    def env_seen(self) -> List[list]:
        path = Path(str(self.argv_file) + ".env")
        return [json.loads(x) for x in path.read_text().splitlines()]

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_SanitizerUnitRunsWithFatalUbsanAndAsanOptions(self) -> None:
        self.stub("ctest", self.ENV_PROBE + "sys.exit(0)\n")
        for runner, asan in (("linux", "detect_container_overflow=0"),
                             ("macos", "detect_leaks=0:detect_container_overflow=0")):
            with self.subTest(runner=runner):
                leg = stage.Leg(f"{runner}-asan", runner, runner, "", unit_tests=True,
                                job="sanitizers")
                ctx = make_ctx(self.tmp, leg, stage_name="test")
                rc, lines = self.capture(stage.step_unit, ctx)
                self.assertEqual(rc, 0)
                self.assertTrue(lines[0].startswith("ok: tests unprivileged"))
                self.assertEqual(self.env_seen()[-1], [asan, "halt_on_error=1:print_stacktrace=1"])
                argv = self.argvs()[-1]
                self.assertEqual(argv[argv.index("--test-dir") + 1],
                                 str(self.tmp / "ws" / "build" / f"{runner}-asan"))
                self.assertNotIn("UBSAN_OPTIONS", os.environ, "options are scoped to the step")

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_PlainLegsGetNoSanitizerOptions(self) -> None:
        self.stub("ctest", self.ENV_PROBE + "sys.exit(0)\n")
        leg = stage.Leg("windows", "windows", "windows", "Debug", unit_tests=True)
        with mock.patch.dict(os.environ, {}, clear=False):
            os.environ.pop("ASAN_OPTIONS", None)
            os.environ.pop("UBSAN_OPTIONS", None)
            self.capture(stage.step_unit, make_ctx(self.tmp, leg, stage_name="test"))
        self.assertEqual(self.env_seen(), [[None, None]])

    def test_SanitizerConfigureAndBuildArgv(self) -> None:
        leg = stage.Leg("linux-asan", "linux", "linux", "", unit_tests=True, job="sanitizers")
        ctx = make_ctx(self.tmp, leg)
        argv = stage.configure_argv(ctx)
        build_dir = str(self.tmp / "ws" / "build" / "linux-asan")
        self.assertEqual(argv[:5], ["cmake", "--preset", "linux", "-B", build_dir])
        self.assertIn("-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -g "
                      "-fno-sanitize-recover=undefined", argv)
        self.assertIn("-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined", argv)
        self.assertEqual(stage.build_argv(ctx), ["cmake", "--build", build_dir])
        plain = make_ctx(self.tmp, stage.Leg("linux", "linux", "linux", ""))
        self.assertEqual(stage.configure_argv(plain), ["cmake", "--preset", "linux"])
        self.assertEqual(stage.build_argv(plain), ["cmake", "--build", "--preset", "linux"])

    def test_SanitizerLegUsesTheAsanCacheFlavor(self) -> None:
        leg = stage.Leg("linux-asan", "linux", "linux", "", job="sanitizers")
        ctx = make_ctx(self.tmp, leg, reporter="github")
        ctx.workspace = REPO_ROOT
        out = self.tmp / "outputs"
        with mock.patch.dict(os.environ, {"CI_OUTPUT_FILE": str(out)}), \
                mock.patch("print_env.probe_cxx", return_value=("gcc", "13.3.0")):
            rc, lines = self.capture(stage.step_cache_key, ctx)
        self.assertEqual(rc, 0, lines)
        self.assertRegex(lines[0], r"^info: vcpkg cache key: vcpkg-asan-ubuntu-24\.04-gcc13-")

    def test_LinuxSanitizerLegSkipsLiveTableAndCliFormats(self) -> None:
        leg = stage.Leg("linux-asan", "linux", "linux", "", unit_tests=True, job="sanitizers")
        ctx = make_ctx(self.tmp, leg, stage_name="test")
        rc, lines = self.capture(stage.step_live_table, ctx)
        self.assertEqual((rc, lines), (0, [f"skip: tests live-table: {stage.LIVE_TABLE_ASAN_SKIP}"]))
        rc, lines = self.capture(stage.step_cli_formats, ctx)
        self.assertEqual(rc, 0)
        self.assertTrue(lines[0].startswith("skip: cli formats: "))
        self.assertEqual([r["outcome"] for r in records(self.tmp)], ["skip", "skip"])

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_LinuxLiveTableRunsUnderSudoOnCi(self) -> None:
        self.stub("ctest", "sys.exit(0)\n")
        self.stub("sudo", "sys.exit(0)\n")
        leg = stage.Leg("linux", "linux", "linux", "", live_table_sudo=True,
                        live_table_tests=True)
        rc, _ = self.capture(stage.step_live_table,
                             make_ctx(self.tmp, leg, platform="github", stage_name="test"))
        self.assertEqual(rc, 0)
        (argv,) = self.argvs()
        self.assertEqual(argv[0], "-n")
        self.assertTrue(argv[1].endswith("ctest"))
        (record,) = records(self.tmp)
        self.assertEqual(record["mode"], "best-effort")

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_LiveTableNeverUsesSudoLocally(self) -> None:
        self.stub("ctest", "sys.exit(0)\n")
        leg = stage.Leg("linux", "linux", "linux", "", live_table_sudo=True,
                        live_table_tests=True)
        self.capture(stage.step_live_table, make_ctx(self.tmp, leg, stage_name="test"))
        (argv,) = self.argvs()
        self.assertEqual(argv[0], "--test-dir")

    def test_StepsOffTheirLegSkip(self) -> None:
        leg = stage.Leg("linux", "linux", "linux", "")
        ctx = make_ctx(self.tmp, leg, stage_name="test")
        for fn, label in ((stage.step_unit, "tests unprivileged"),
                          (stage.step_live_table, "tests live-table"),
                          (stage.step_install, "install tree")):
            with self.subTest(step=label):
                rc, lines = self.capture(fn, ctx)
                self.assertEqual(rc, 0)
                self.assertEqual(len(lines), 1)
                self.assertTrue(lines[0].startswith(f"skip: {label}: "))

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_InstallExportsDigestAsJobOutput(self) -> None:
        self.stub("cmake", "prefix = sys.argv[sys.argv.index('--prefix') + 1]\n"
                           "os.makedirs(os.path.join(prefix, 'lib'))\n"
                           "open(os.path.join(prefix, 'lib', 'libsmbios_c.a'), 'wb')"
                           ".write(b'x')\n")
        out = self.tmp / "outputs"
        leg = stage.Leg("linux-x64", "linux", "linux", "Release", packaging=True)
        ctx = make_ctx(self.tmp, leg, reporter="github")
        with mock.patch.dict(os.environ, {"CI_OUTPUT_FILE": str(out)}):
            rc, lines = self.capture(stage.step_install, ctx)
        self.assertEqual(rc, 0, lines)
        self.assertEqual(len(lines), 1)
        self.assertRegex(lines[0], r"^ok: install tree: 1 files, sha256 [0-9a-f]{16} ")
        (argv,) = self.argvs()
        self.assertEqual(argv[:2], ["--install", str(self.tmp / "ws" / "build" / "linux")])
        self.assertEqual(argv[argv.index("--config") + 1], "Release")
        prefix = self.tmp / "artifacts" / "install" / "linux-x64"
        digest, count = stage.tree_digest(prefix)
        text = out.read_text(encoding="ascii")
        self.assertIn("install_digest<<", text)
        value = json.loads(text.splitlines()[1])
        self.assertEqual(value, {"files": count, "leg": "linux-x64", "sha256": digest})

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_InstallWithTestingTreeFails(self) -> None:
        self.stub("cmake", "prefix = sys.argv[sys.argv.index('--prefix') + 1]\n"
                           "os.makedirs(os.path.join(prefix, 'Testing'))\n"
                           "open(os.path.join(prefix, 'Testing', 'LastTest.log'), 'w')"
                           ".write('TEST-SERIAL-0000')\n")
        leg = stage.Leg("linux-x64", "linux", "linux", "Release", packaging=True)
        out = self.tmp / "outputs"
        with mock.patch.dict(os.environ, {"CI_OUTPUT_FILE": str(out)}):
            rc, lines = self.capture(stage.step_install, make_ctx(self.tmp, leg))
        self.assertEqual(rc, 1)
        self.assertIn("Testing path", lines[0])
        self.assertNotIn("TEST-SERIAL-0000", "\n".join(lines))
        self.assertFalse(out.exists(), "no digest is exported for a refused tree")

    def test_TagEventPackagingLegBuildsCold(self) -> None:
        out = self.tmp / "outputs"
        leg = stage.Leg("linux-x64", "linux", "linux", "Release", packaging=True)
        ctx = make_ctx(self.tmp, leg, reporter="github", event="tag", ref="refs/tags/v1.0.0")
        self.assertTrue(stage.cold_build(ctx))
        with mock.patch.dict(os.environ, {"CI_OUTPUT_FILE": str(out)}):
            rc, lines = self.capture(stage.step_cache_key, ctx)
        self.assertEqual(rc, 0)
        self.assertEqual(lines, ["skip: vcpkg cache: tag-event packaging leg builds cold (CR-2)"])
        self.assertIn("\nfalse\n", out.read_text(encoding="ascii"))
        self.assertFalse(stage.cold_build(make_ctx(self.tmp, stage.Leg("linux", "linux", "linux",
                                                                       ""), event="tag")))

    def test_CacheKeyIsExportedForNormalLegs(self) -> None:
        out = self.tmp / "outputs"
        ctx = make_ctx(self.tmp, stage.Leg("linux", "linux", "linux", ""), reporter="github",
                       event="push", ref="refs/heads/master")
        ctx.workspace = REPO_ROOT
        with mock.patch.dict(os.environ, {"CI_OUTPUT_FILE": str(out)}), \
                mock.patch("print_env.probe_cxx", return_value=("gcc", "13.3.0")):
            rc, lines = self.capture(stage.step_cache_key, ctx)
        self.assertEqual(rc, 0)
        self.assertRegex(lines[0], r"^info: vcpkg cache key: vcpkg-default-ubuntu-24\.04-gcc13-"
                                   r"[0-9a-f]{16}-local$")
        text = out.read_text(encoding="ascii")
        self.assertIn("\ntrue\n", text)
        self.assertIn('"restore_prefix":"vcpkg-default-ubuntu-24.04-gcc13-', text)

    def test_OversizedUploadLogIsRenamed(self) -> None:
        ctx = make_ctx(self.tmp, stage.Leg("linux", "linux", "linux", ""))
        ctx.log_dir.mkdir()
        big = ctx.log_dir / "build.log"
        with open(big, "wb") as fh:
            fh.truncate(stage.LOG_UPLOAD_CAP + 1)  # sparse: fast
        (ctx.log_dir / "configure.log").write_text("small\n", encoding="utf-8")
        rc, lines = self.capture(lambda c: stage.cap_upload_logs(c) or 0, ctx)
        self.assertEqual(lines, ["info: build.log is 50 MiB, above the 50 MiB upload cap; "
                                 "not uploaded"])
        self.assertFalse(big.exists())
        self.assertTrue((ctx.log_dir / "build.log.over-cap").exists())
        self.assertTrue((ctx.log_dir / "configure.log").exists())

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_CompileStopsAtConfigureFailure(self) -> None:
        self.stub("cmake", "print('CMake Error at CMakeLists.txt:1 (project): boom')\n"
                           "sys.exit(1)\n")
        ctx = make_ctx(self.tmp, stage.Leg("linux", "linux", "linux", ""), stage_name="build")
        with mock.patch("bootstrap_vcpkg.main", return_value=0), \
                mock.patch("vcpkg_cache_report.main", return_value=0):
            rc, lines = self.capture(stage.step_compile, ctx)
        self.assertEqual(rc, 1)
        self.assertTrue(lines[0].startswith("error: configure failed (exit 1"), lines)
        self.assertEqual([a[0] for a in self.argvs()], ["--preset"], "build never ran")

    def test_BootstrapFailureStopsCompile(self) -> None:
        ctx = make_ctx(self.tmp, stage.Leg("linux", "linux", "linux", ""))
        with mock.patch("bootstrap_vcpkg.main", return_value=1) as boot:
            rc, _ = self.capture(stage.step_compile, ctx)
        self.assertEqual(rc, 1)
        self.assertNotIn("--packaging-leg", boot.call_args.args[0])

    # --- Story 03.0 Task 03: GUI legs (C15-C20) -------------------------------

    GUI = stage.Leg("linux-gui", "linux", "linux-gui", "Debug", job="gui", apt_packages=True)

    def test_GuiLegsData(self) -> None:
        data = stage.load_legs()
        for name, runner in (("linux-gui", "linux"), ("windows-gui", "windows"),
                             ("macos-gui", "macos")):
            with self.subTest(leg=name):
                leg = stage._leg(name, data["legs"][name])
                self.assertTrue(leg.gui and not leg.sanitizers and not leg.packaging)
                self.assertEqual((leg.runner, leg.preset, leg.config), (runner, name, "Debug"))
                self.assertEqual(leg.apt_packages, runner == "linux", "C15 is Linux only")
                self.assertFalse(leg.unit_tests or leg.live_table_tests or leg.live_table_sudo)
                self.assertIn(name, data["stages"]["build"])
                self.assertIn(name, data["stages"]["test"])
                self.assertEqual(stage.steps_for("build", leg), ("packages", "prepare", "compile"))
                self.assertEqual(stage.steps_for("test", leg),
                                 ("gui-binary", "gui-hooks", "gui-ci", "gui-local"))

    def test_GuiLegBuildsThePresetTreeWithTheGuiCacheFlavor(self) -> None:
        ctx = make_ctx(self.tmp, self.GUI, reporter="github")
        self.assertEqual(stage.configure_argv(ctx), ["cmake", "--preset", "linux-gui"])
        self.assertEqual(stage.build_argv(ctx), ["cmake", "--build", "--preset", "linux-gui"])
        self.assertEqual(ctx.build_dir, self.tmp / "ws" / "build" / "linux-gui")
        ctx.workspace = REPO_ROOT
        with mock.patch.dict(os.environ, {"CI_OUTPUT_FILE": str(self.tmp / "outputs")}), \
                mock.patch("print_env.probe_cxx", return_value=("gcc", "13.3.0")):
            rc, lines = self.capture(stage.step_cache_key, ctx)
        self.assertEqual(rc, 0, lines)
        self.assertRegex(lines[0], r"^info: vcpkg cache key: vcpkg-gui-ubuntu-24\.04-gcc13-")

    def test_PackagesStepRunsAptInstallOnlyOnCiLinuxGui(self) -> None:
        with mock.patch("apt_install.main", return_value=0) as apt:
            rc, _ = self.capture(stage.step_packages,
                                 make_ctx(self.tmp, self.GUI, platform="github"))
            self.assertEqual(rc, 0)
            self.assertEqual(apt.call_args.args[0], ["--reporter", "plain"])
            apt.return_value = 100
            rc, _ = self.capture(stage.step_packages,
                                 make_ctx(self.tmp, self.GUI, platform="github"))
            self.assertEqual(rc, 100, "an apt failure fails the step")
            apt.reset_mock()
            rc, lines = self.capture(stage.step_packages, make_ctx(self.tmp, self.GUI))
            self.assertEqual(rc, 0)
            self.assertTrue(lines[0].startswith("skip: apt packages: local run"))
            mac = stage.Leg("macos-gui", "macos", "macos-gui", "Debug", job="gui")
            rc, lines = self.capture(stage.step_packages,
                                     make_ctx(self.tmp, mac, platform="github"))
            self.assertTrue(lines[0].startswith("skip: apt packages: "))
            apt.assert_not_called()

    def _gui_tree(self, hook: bool = False) -> Path:
        """A configured GUI build tree: binary above the R-7 floor, CTestTestfile.cmake (S3-2/3)."""
        import gui_guards
        bindir = self.tmp / "ws" / "build" / "linux-gui" / "smbios_gui" / "bin"
        bindir.mkdir(parents=True)
        (bindir / "smbios_gui").write_bytes(b"\x7fELF" + (b"--exit-after-frames" if hook else b"")
                                            + b"\0" * gui_guards.MIN_BINARY_BYTES)
        (bindir.parents[1] / gui_guards.CTEST_FILE).write_text("", encoding="ascii")
        return bindir.parents[1]

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_GuiTestStageRunsEveryCheckInOrder(self) -> None:
        self._gui_tree()
        self.stub("ctest", "if '-N' in sys.argv:\n"
                           "    print('Test project x')\n    print('')\n"
                           "    print('Total Tests: 3' if 'gui-ci' in sys.argv else "
                           "'Total Tests: 0')\n"
                           "sys.exit(0)\n")
        ctx = make_ctx(self.tmp, self.GUI, stage_name="test")
        rc, lines = self.capture(stage.run_stage, ctx, None)
        self.assertEqual(rc, 0, lines)
        self.assertEqual([x.split(":")[1].strip() for x in lines if x.startswith("ok:")][:1],
                         ["smbios_gui"])
        self.assertTrue(any(x.startswith("ok: no test hook strings in the shipped smbios_gui")
                            for x in lines), lines)
        self.assertTrue(any(x.startswith("ok: tests gui-ci") for x in lines), lines)
        self.assertTrue(any(x.startswith("ok: no gui-local tests configured") for x in lines),
                        lines)
        gui_ci = [a for a in self.argvs() if "-N" not in a]
        self.assertEqual(len(gui_ci), 1)
        (argv,) = gui_ci
        self.assertEqual(argv[argv.index("-L") + 1], "gui-ci")
        self.assertEqual(argv[argv.index("-C") + 1], "Debug")
        self.assertIn("--no-tests=error", argv)
        self.assertIn("--output-on-failure", argv, "gui-ci is fixture-free (0011 R-5)")
        listings = [a for a in self.argvs() if "-N" in a]
        self.assertEqual([a[a.index("-N") + 1:] for a in listings],
                         [["-L", "gui-local"], ["-R", "smbios_gui_local_test|smbios_gui_autoexit"],
                          ["-L", "gui-ci"]])
        self.assertTrue(all(a[a.index("-C") + 1] == "Debug" for a in listings))
        for argv in self.argvs():
            self.assertFalse(any(x.endswith("smbios_gui") for x in argv), "never runs smbios_gui")

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_GuiTestStageFailsOnHookAndStillRunsLaterChecks(self) -> None:
        self._gui_tree(hook=True)
        self.stub("ctest", "print(('Total Tests: 3' if 'gui-ci' in sys.argv else 'Total Tests: 0')"
                           " if '-N' in sys.argv else None)\nsys.exit(0)\n")
        ctx = make_ctx(self.tmp, self.GUI, stage_name="test")
        rc, lines = self.capture(stage.run_stage, ctx, None)
        self.assertEqual(rc, 1)
        self.assertIn("test hook present in shipped smbios_gui: expected 0 -> 1", "\n".join(lines))
        self.assertTrue(any(x.startswith("ok: no gui-local tests configured") for x in lines))

    @unittest.skipIf(os.name == "nt", POSIX_ONLY)
    def test_GuiLocalOnAnUnconfiguredTreeFails(self) -> None:
        """S3-3: real ctest answers ``Total Tests: 0`` for any directory; that is not a pass."""
        self.stub("ctest", "print('Total Tests: 0') if '-N' in sys.argv else None\nsys.exit(0)\n")
        rc, lines = self.capture(stage.step_gui_local, make_ctx(self.tmp, self.GUI))
        self.assertEqual(rc, 1, lines)
        self.assertFalse(any(x.startswith("ok:") for x in lines), lines)

    def test_GuiHooksOnAnEmptyBinaryFails(self) -> None:
        """S3-2 through the stage step: a 0-byte smbios_gui is not ``ok:``."""
        bindir = self.tmp / "ws" / "build" / "linux-gui" / "smbios_gui" / "bin"
        bindir.mkdir(parents=True)
        (bindir / "smbios_gui").write_bytes(b"")
        rc, lines = self.capture(stage.step_gui_hooks, make_ctx(self.tmp, self.GUI))
        self.assertEqual(rc, 1)
        self.assertIn("smbios_gui binary size: expected >= ", lines[0])

    def test_GuiHooksWithoutABinaryFailsClosed(self) -> None:
        rc, lines = self.capture(stage.step_gui_hooks, make_ctx(self.tmp, self.GUI))
        self.assertEqual(rc, 1)
        self.assertIn("smbios_gui binary: expected 1 -> actual 0", lines[0])

    def test_OnlyAStepOfAnotherJobSkips(self) -> None:
        rc, lines = self.capture(stage.run_stage, make_ctx(self.tmp, self.GUI, stage_name="test"),
                                 "unit")
        self.assertEqual((rc, lines), (0, ["skip: test unit: not a step of the gui job"]))
        plain = make_ctx(self.tmp, stage.Leg("linux", "linux", "linux", ""), stage_name="build")
        rc, lines = self.capture(stage.run_stage, plain, "packages")
        self.assertEqual((rc, lines), (0, ["skip: build packages: not a step of the build job"]))


class RunStageTests(unittest.TestCase):
    """Stage sequencing and the SC1 exit-code mapping."""

    def ctx(self, stage_name: str) -> stage.Context:
        return make_ctx(Path(tempfile.gettempdir()), stage.Leg("linux", "linux", "linux", ""),
                        stage_name=stage_name)

    def _run(self, stage_name: str, codes: Dict[str, int]) -> tuple:
        called: List[str] = []

        def fake(name: str):
            def fn(_ctx: stage.Context) -> int:
                called.append(name)
                return codes.get(name, 0)
            return fn
        table = {stage_name: {n: fake(n) for n in stage.STEPS[stage_name]}}
        with mock.patch.dict(stage.STEP_FUNCTIONS, table):
            rc = stage.run_stage(self.ctx(stage_name), None)
        return rc, called

    def test_LintRunsEveryCheckEvenAfterAFailure(self) -> None:
        rc, called = self._run("lint", {"lint": 1})
        self.assertEqual(rc, 1)
        self.assertEqual(called, list(stage.STEPS["lint"]))

    def test_BuildStopsAtTheFirstFailure(self) -> None:
        rc, called = self._run("build", {"prepare": 2})
        self.assertEqual(rc, 1, "a failing sub-step is a gate failure, not a usage error")
        self.assertEqual(called, ["prepare"])

    def test_SignalExitIsPropagated(self) -> None:
        rc, _ = self._run("test", {"unit": 137})
        self.assertEqual(rc, 137)

    def test_OnlyRunsOneStep(self) -> None:
        called: List[str] = []
        table = {"test": {n: (lambda n: (lambda c: called.append(n) or 0))(n)
                          for n in stage.STEPS["test"]}}
        with mock.patch.dict(stage.STEP_FUNCTIONS, table):
            self.assertEqual(stage.run_stage(self.ctx("test"), "cli-formats"), 0)
        self.assertEqual(called, ["cli-formats"])

    def test_EveryStepHasAFunction(self) -> None:
        for name, steps in stage.STEPS.items():
            self.assertEqual(tuple(stage.STEP_FUNCTIONS[name]), steps)


class ToolsVenvTests(unittest.TestCase):
    """The job-local tools venv is only activated when it is a real directory."""

    def test_ActivateOnlyARealDirectory(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_name, \
                mock.patch.dict(os.environ, {"PATH": "/usr/bin"}):
            tmp = Path(tmp_name)
            self.assertFalse(stage.activate_tools(tmp))
            if os.name != "nt":
                elsewhere = tmp / "elsewhere"
                (elsewhere / "bin").mkdir(parents=True)
                (tmp / stage.TOOLS_VENV).symlink_to(elsewhere, target_is_directory=True)
                self.assertFalse(stage.activate_tools(tmp))
                (tmp / stage.TOOLS_VENV).unlink()
            stage._venv_bin(tmp).mkdir(parents=True)
            self.assertTrue(stage.activate_tools(tmp))
            self.assertTrue(os.environ["PATH"].startswith(str(stage._venv_bin(tmp))))
            stage.activate_tools(tmp)
            self.assertEqual(os.environ["PATH"].count(str(stage._venv_bin(tmp))), 1)


if __name__ == "__main__":
    unittest.main()
