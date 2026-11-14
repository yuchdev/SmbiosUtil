"""Tests for the RC4 version header and the toolchain pins (milestone 0014, Story 02.0 Task 01).

Covers ``scripts/ci/print_env.py`` (probe results ``-``/``?``, the 240-char
ceiling, the probe timeout, the CR-4 version-token allowlist, the CR-11
hand-off file), the minimal ``Reporter.info`` event it uses, and the
consistency of ``scripts/ci/pins.json`` with ``requirements-tools.txt`` and the
vcpkg manifests.

Classification: unit (stdlib only, no network, no host cmake/ctest - every
tool is a stub or a name that cannot exist).
On failure first check: a stub's output pattern versus the probe regex in
print_env.py; for the pins cases, which of the three files was edited alone.

Run: ``python3 scripts/ci/run_tests.py`` (all of ``scripts/ci/tests``).
"""

from __future__ import annotations

import json
import os
import re
import stat
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from typing import List

from ci_test_support import CI_DIR, neutral_env, stub_argv, write_stub

import print_env  # noqa: E402  (ci_test_support put scripts/ci on sys.path)
import reporters  # noqa: E402

REPO_ROOT = CI_DIR.parents[1]
MISSING_TOOL = "smbios-ci-no-such-tool-7f3a"
#: Hostile banners (CR-4); built by concatenation so the P11 scan of the
#: harness's siblings never sees them verbatim in a non-test file.
HOSTILE = ("::" + "error::x", "##" + "teamcity[buildStatus text='x']")
STUB_DIR_POSIX_ONLY = os.name == "nt"


def _stub(tmp: Path, name: str, body: str) -> List[str]:
    """Write stub ``name`` and return an argv that runs it by absolute path."""
    write_stub(tmp, name, body)
    if os.name == "nt":
        return stub_argv(tmp, name)
    return [str(tmp / name)]


def _printer(text: str) -> str:
    return f"import sys\nsys.stdout.write({text!r} + '\\n')\n"


class _TmpCase(unittest.TestCase):
    def setUp(self) -> None:
        self._td = tempfile.TemporaryDirectory()
        self.tmp = Path(self._td.name)

    def tearDown(self) -> None:
        self._td.cleanup()


class ProbeTests(_TmpCase):
    """``probe``: version, ``-`` for missing/hung tools, ``?`` for unrecognised output."""

    def test_NormalVersionIsReturned(self) -> None:
        argv = _stub(self.tmp, "cmake", _printer("cmake version 4.4.4\n\nCMake suite"))
        self.assertEqual(print_env.probe(argv, r"cmake version (\S+)"), "4.4.4")

    def test_MissingToolGivesDash(self) -> None:
        self.assertEqual(print_env.probe([MISSING_TOOL, "--version"], r"(\S+)"), "-")

    def test_HungProbeGivesDashWithinTimeout(self) -> None:
        argv = _stub(self.tmp, "hang", "import time\ntime.sleep(60)\n")
        start = time.monotonic()
        self.assertEqual(print_env.probe(argv, r"(\S+)", timeout_s=1.0), "-")
        self.assertLess(time.monotonic() - start, 8.0)

    def test_HostileBannerGivesQuestionMark(self) -> None:
        for i, text in enumerate(HOSTILE):
            with self.subTest(text=text):
                argv = _stub(self.tmp, f"tool{i}", _printer(text))
                self.assertEqual(print_env.probe(argv, r"^\s*(\S+)"), "?")
                self.assertEqual(print_env.probe(argv, r"cmake version (\S+)"), "?")

    def test_HostileTokenInVersionSlotGivesQuestionMark(self) -> None:
        argv = _stub(self.tmp, "cmake", _printer("cmake version " + HOSTILE[0]))
        self.assertEqual(print_env.probe(argv, r"cmake version (\S+)"), "?")

    def test_OverlongTokenGivesQuestionMark(self) -> None:
        argv = _stub(self.tmp, "ninja", _printer("1" * 41))
        self.assertEqual(print_env.probe(argv, r"^\s*(\S+)"), "?")


class EnvLineTests(_TmpCase):
    """``env_line``: RC4 shape, ceiling and allowlists."""

    def _line(self, job: str = "build", leg: str = "linux-x64", **kw) -> str:
        old = os.environ.get("PATH", "")
        os.environ["PATH"] = str(self.tmp / "empty")  # no tool can be found
        try:
            return print_env.env_line(job, leg, timeout_s=2.0, **kw)
        finally:
            os.environ["PATH"] = old

    def test_MissingToolsGiveDashes(self) -> None:
        line = self._line(image="ubuntu24 20261101.360.1", pins={})
        self.assertTrue(line.startswith("env: build/linux-x64 image=ubuntu24/20261101.360.1 | "))
        for field in ("cmake -", "ctest -", "ninja -", "vcpkg ?", "cache -"):
            self.assertIn(f"| {field}", line)

    def test_PinnedVcpkgIsFirst12Chars(self) -> None:
        line = self._line(image="", pins={"vcpkg": {"commit": "0123456789abcdef" * 2 + "01234567"}})
        self.assertIn("| vcpkg 0123456789ab |", line)
        self.assertIn("image=- |", line)

    def test_LineNeverExceeds240Chars(self) -> None:
        line = self._line(job="j" * 500, leg="l" * 500, image="i" * 500, cache="partial",
                          pins={"vcpkg": {"commit": "a" * 40}})
        self.assertLessEqual(len(line), 240)
        long_ok = self._line(job="j" * 40, leg="l" * 40, image="i" * 60, cache="partial",
                             pins={"vcpkg": {"commit": "a" * 40}})
        self.assertLessEqual(len(long_ok), 240)

    def test_HostileJobLegImageAreReplaced(self) -> None:
        line = self._line(job=HOSTILE[0], leg="a\nb", image=HOSTILE[1], pins={})
        self.assertTrue(line.startswith("env: ?/? image=? | "))
        for text in HOSTILE:
            self.assertNotIn(text, line)

    def test_UnknownCacheStateIsDash(self) -> None:
        self.assertIn("| cache -", self._line(cache="bogus", pins={}))


@unittest.skipIf(STUB_DIR_POSIX_ONLY, "PATH stubs need the POSIX trampoline")
class MainEndToEndTests(_TmpCase):
    """``print_env.py`` as a process: hostile tools never reach stdout, exit is 0."""

    def _run(self, reporter: str) -> subprocess.CompletedProcess:
        bin_dir = self.tmp / "bin"
        for i, name in enumerate(("cmake", "ctest", "ninja", "c++")):
            write_stub(bin_dir, name, _printer(HOSTILE[i % 2]))
        env = neutral_env(self.tmp, PATH=str(bin_dir), CI_REPORTER=reporter,
                          CI_ARTIFACT_DIR=str(self.tmp / "art"),
                          CI_SUMMARY_FILE=str(self.tmp / "summary.md"),
                          CI_IMAGE="ubuntu24 20261101.360.1")
        return subprocess.run([sys.executable, "-I", str(CI_DIR / "print_env.py"),
                               "--job", "build", "--leg", "linux-x64"],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              stdin=subprocess.DEVNULL, env=env, timeout=120)

    def test_HostileToolsNeverReachOutput(self) -> None:
        for backend in reporters.BACKEND_NAMES:
            with self.subTest(backend=backend):
                proc = self._run(backend)
                self.assertEqual(proc.returncode, 0, proc.stderr)
                out = proc.stdout.decode("utf-8")
                lines = out.splitlines()
                self.assertTrue(lines and lines[0].startswith("env: build/linux-x64 "), out)
                self.assertIn("| cmake ? | ctest ? | ninja ? | cxx ? ? |", lines[0])
                self.assertLessEqual(len(lines[0]), 240)
                for text in HOSTILE:
                    self.assertNotIn(text, out)
                handed = (self.tmp / print_env.ENV_LINE_FILE).read_text(encoding="utf-8")
                self.assertEqual(handed, lines[0] + "\n")

    def test_SummaryFirstLineIsTheEnvLine(self) -> None:
        proc = self._run("github")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        summary = (self.tmp / "summary.md").read_text(encoding="utf-8").splitlines()
        self.assertEqual(summary[0], proc.stdout.decode("utf-8").splitlines()[0])


class HandOffFileTests(_TmpCase):
    """CR-11: the ``CI_ENV_LINE`` hand-off file is private and never follows a symlink."""

    def test_FileHoldsLineAndIsPrivate(self) -> None:
        path = print_env.write_env_line("env: x/y", self.tmp)
        self.assertEqual(path.read_text(encoding="utf-8"), "env: x/y\n")
        if os.name != "nt":
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)

    def test_ExistingWideModeFileIsTightened(self) -> None:
        path = self.tmp / print_env.ENV_LINE_FILE
        path.write_text("old\n", encoding="utf-8")
        if os.name != "nt":
            path.chmod(0o644)
        print_env.write_env_line("env: new", self.tmp)
        self.assertEqual(path.read_text(encoding="utf-8"), "env: new\n")
        if os.name != "nt":
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)

    @unittest.skipIf(os.name == "nt", "symlink creation needs privileges on Windows")
    def test_SymlinkIsRefusedAndTargetUnchanged(self) -> None:
        target = self.tmp / "victim.txt"
        target.write_text("keep\n", encoding="utf-8")
        (self.tmp / print_env.ENV_LINE_FILE).symlink_to(target)
        with self.assertRaises(OSError):
            print_env.write_env_line("env: x/y", self.tmp)
        self.assertEqual(target.read_text(encoding="utf-8"), "keep\n")

    def test_PythonFloorIs39(self) -> None:
        self.assertFalse(print_env.python_supported((3, 8)))
        self.assertTrue(print_env.python_supported((3, 9)))
        self.assertTrue(print_env.python_supported((4, 0)))


class InfoEventTests(unittest.TestCase):
    """The minimal ``Reporter.info`` event: sanitised, one line, never interpretable."""

    HOSTILE_INFO = ("::" + "error::x", "##[error]x", "  ::" + "warning::y",
                    "ok: x ##" + "teamcity[buildStatus text='green']",
                    "a\x1b[31mred\x1b[0m\nsecond line", "z" * 400)

    def test_EveryBackendSanitises(self) -> None:
        for backend in reporters.BACKEND_NAMES:
            rep = reporters.get_reporter(backend)
            for text in self.HOSTILE_INFO:
                with self.subTest(backend=backend, text=text[:20]):
                    lines = rep.info(text)
                    self.assertEqual(len(lines), 1)
                    line = lines[0]
                    self.assertFalse(line.lstrip().startswith(("::", "##[")), line)
                    self.assertNotIn("\x1b", line)
                    self.assertNotIn("second line", line)
                    self.assertLessEqual(len(line), reporters.INFO_MAX)
                    if backend == "teamcity":
                        self.assertNotIn("##" + "teamcity[", line)

    def test_CleanLineIsUnchanged(self) -> None:
        line = "env: build/linux-x64 image=- | cmake 4.4.4"
        for backend in reporters.BACKEND_NAMES:
            self.assertEqual(reporters.get_reporter(backend).info(line), [line])


class PinsConsistencyTests(unittest.TestCase):
    """``pins.json`` agrees with ``requirements-tools.txt`` and both vcpkg manifests."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.pins = json.loads((CI_DIR / "pins.json").read_text(encoding="utf-8"))
        cls.reqs = (CI_DIR / "requirements-tools.txt").read_text(encoding="utf-8")

    def test_VcpkgCommitEqualsBothManifestBaselines(self) -> None:
        manifest = json.loads((REPO_ROOT / "vcpkg.json").read_text(encoding="utf-8"))
        config = json.loads((REPO_ROOT / "vcpkg-configuration.json").read_text(encoding="utf-8"))
        commit = self.pins["vcpkg"]["commit"]
        self.assertRegex(commit, r"^[0-9a-f]{40}$")
        self.assertEqual(commit, manifest["builtin-baseline"])
        self.assertEqual(commit, config["default-registry"]["baseline"])

    def test_RunnerLabelsAreExplicit(self) -> None:
        runners = self.pins["runners"]
        self.assertEqual(set(runners), {"linux", "macos", "windows"})
        for label in runners.values():
            self.assertRegex(label, r"^[a-z0-9.-]+$")
            self.assertFalse(label.endswith("-latest") or "latest" in label, label)

    def test_ToolVersionsMatchRequirements(self) -> None:
        pinned = dict(re.findall(r"^([a-z]+)==(\S+)", self.reqs, re.MULTILINE))
        self.assertEqual(pinned, {"cmake": self.pins["tools"]["cmake"],
                                  "ninja": self.pins["tools"]["ninja"]})
        self.assertTrue(self.pins["tools"]["cmake"].startswith("4."))

    def test_EveryRequirementHasOneHashPerRunnerPlatform(self) -> None:
        blocks = re.split(r"^(?=[a-z]+==)", self.reqs, flags=re.MULTILINE)[1:]
        self.assertEqual(len(blocks), 2)
        for block in blocks:
            hashes = re.findall(r"--hash=sha256:([0-9a-f]{64})\b", block)
            self.assertEqual(len(hashes), 3, block)
            self.assertEqual(len(set(hashes)), 3, block)

    def test_InstallCommandRequiresBinaryOnly(self) -> None:
        self.assertIn("--only-binary :all:", self.reqs)
        self.assertIn("--require-hashes", self.reqs)


if __name__ == "__main__":
    unittest.main()
