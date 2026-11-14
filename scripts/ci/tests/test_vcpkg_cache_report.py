"""Tests for the vcpkg cache health line and key scheme (milestone 0014, Story 02.0 Task 02).

Covers ``scripts/ci/vcpkg_cache_report.py``: archive counting (missing,
empty, symlinked entries), the tolerant configure-log parse, the
``exact|partial|miss`` classification, the single ``info: vcpkg cache ...``
line (including ``built-from-source unknown``), the cache key and restore
prefix, and the save rule shared with ``bootstrap_vcpkg.py`` (CR-2).

Classification: unit (pure functions) plus one CLI run of ``snapshot`` +
``report``. Stdlib only, no network, synthetic logs only.
On failure first check: the vcpkg log wording in the synthetic logs versus
``_RESTORED``/``_BUILDING``/``_MARKERS``; vcpkg may have changed its wording,
which the parse must tolerate (it then says ``unknown``, never fails).
Whether the real configure log yields a cache hit/miss line is verified on a
real runner (Story 03.0), not here.

Run: ``python3 scripts/ci/run_tests.py``.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from ci_test_support import CI_DIR, neutral_env

import vcpkg_cache_report as vcr  # noqa: E402  (ci_test_support put scripts/ci on sys.path)

SCRIPT = CI_DIR / "vcpkg_cache_report.py"
REPO_ROOT = CI_DIR.parents[1]
_LINE = re.compile(r"^info: vcpkg cache (exact|partial|miss), \d+ archives, \d+\.\d MB, "
                   r"(\d+ packages built from source|built-from-source unknown)$")

LOG_BUILT = """\
-- Running vcpkg install
Detecting compiler hash for triplet x64-linux...
The following packages will be built and installed:
    zlib:x64-linux@1.3.1
    boost-test:x64-linux@1.86.0
Restored 0 package(s) from /tmp/cache in 1.2 ms. Use --debug to see more details.
Installing 1/2 zlib:x64-linux@1.3.1...
Building zlib:x64-linux@1.3.1...
Installing 2/2 boost-test:x64-linux@1.86.0...
Building boost-test:x64-linux@1.86.0...
All requested installations completed successfully in: 2.1 min
"""
LOG_RESTORED = """\
Restored 12 package(s) from /tmp/cache in 640 ms. Use --debug to see more details.
Installing 1/12 zlib:x64-linux@1.3.1...
All requested installations completed successfully in: 3 s
"""
LOG_PARTIAL = LOG_RESTORED.replace("Installing 1/12", "Building zlib:x64-linux@1.3.1...\nInstalling 1/12")


class _Tmp(unittest.TestCase):
    def setUp(self) -> None:
        self._td = tempfile.TemporaryDirectory()
        self.addCleanup(self._td.cleanup)
        self.tmp = Path(self._td.name)

    def log(self, text: str) -> Path:
        path = self.tmp / "configure.log"
        path.write_text(text, encoding="utf-8")
        return path

    def archives(self, n: int, size: int = 1024) -> Path:
        cache = self.tmp / "cache"
        for i in range(n):
            sub = cache / f"{i:02x}"
            sub.mkdir(parents=True, exist_ok=True)
            (sub / f"{i:064x}.zip").write_bytes(b"\0" * size)
        cache.mkdir(exist_ok=True)
        return cache


class CacheReportTests(_Tmp):
    def test_MissingOrEmptyCacheDirIsMiss(self) -> None:
        for cache in (self.tmp / "absent", self.archives(0)):
            with self.subTest(cache=cache.name):
                self.assertEqual(vcr.count_archives(cache), (0, 0))
                line = vcr.report_line(cache, self.log(LOG_BUILT), before_files=0)
                self.assertEqual(line, "info: vcpkg cache miss, 0 archives, 0.0 MB, "
                                       "2 packages built from source")

    def test_UnparseableLogIsUnknown(self) -> None:
        for log in (self.log("nothing vcpkg-shaped here\n\x1b[31mnoise\n"), self.tmp / "none.log"):
            with self.subTest(log=log.name):
                line = vcr.report_line(self.archives(0), log, before_files=0)
                self.assertTrue(line.endswith(", built-from-source unknown"), line)
                self.assertRegex(line, _LINE)

    def test_ExactPartialMiss(self) -> None:
        cache = self.archives(3, size=1048576)
        exact = vcr.report_line(cache, self.log(LOG_RESTORED), before_files=3)
        self.assertEqual(exact, "info: vcpkg cache exact, 3 archives, 3.0 MB, "
                                "0 packages built from source")
        partial = vcr.report_line(cache, self.log(LOG_PARTIAL), before_files=3)
        self.assertTrue(partial.startswith("info: vcpkg cache partial,"), partial)
        self.assertEqual(vcr.classify(3, 0, 2), "miss")
        self.assertEqual(vcr.classify(3, None, None), "partial")

    def test_SymlinksAndNonArchivesNotCounted(self) -> None:
        cache = self.archives(1)
        (cache / "note.txt").write_text("x")
        if hasattr(os, "symlink"):
            try:
                os.symlink(str(next(cache.rglob("*.zip"))), str(cache / "link.zip"))
            except OSError:
                pass
        self.assertEqual(vcr.count_archives(cache), (1, 1024))

    def test_HostileLogLineNeverEchoed(self) -> None:
        hostile = "::" + "error::Building x:y"
        line = vcr.report_line(self.archives(0), self.log(LOG_BUILT + hostile + "\n"), 0)
        self.assertNotIn("::", line)
        self.assertRegex(line, _LINE)


class CacheKeyTests(_Tmp):
    def test_KeyAndRestorePrefix(self) -> None:
        key, prefix = vcr.cache_key("gui", "ubuntu-24.04", "gcc13", REPO_ROOT, "4242")
        self.assertRegex(key, r"^vcpkg-gui-ubuntu-24\.04-gcc13-[0-9a-f]{16}-4242$")
        self.assertEqual(key, prefix + "-4242")

    def test_HashCoversRequirementsTools(self) -> None:
        for rel in vcr.KEY_FILES:
            (self.tmp / rel).parent.mkdir(parents=True, exist_ok=True)
            (self.tmp / rel).write_text("v1")
        before = vcr.key_hash(self.tmp)
        (self.tmp / "scripts/ci/requirements-tools.txt").write_text("v2")
        self.assertNotEqual(vcr.key_hash(self.tmp), before)

    def test_InvalidInputsRejected(self) -> None:
        with self.assertRaises(ValueError):
            vcr.cache_key("release", "ubuntu-24.04", "gcc13", REPO_ROOT, "1")
        with self.assertRaises(ValueError):
            vcr.cache_key("gui", "ubuntu 24.04", "gcc13", REPO_ROOT, "1")

    def test_CxxTag(self) -> None:
        self.assertEqual(vcr.cxx_tag("gcc", "13.3.0"), "gcc13")
        self.assertEqual(vcr.cxx_tag("appleclang", "17.0.0.17000013"), "appleclang17")
        self.assertEqual(vcr.cxx_tag("-", "-"), "unknown")

    def test_SaveOnlyOnMasterPushOrSchedule(self) -> None:
        self.assertTrue(vcr.save_allowed("push", "refs/heads/master"))
        self.assertTrue(vcr.save_allowed("schedule", "refs/heads/master"))
        for event, ref in (("pr", "refs/heads/master"), ("push", "refs/heads/dev"),
                           ("tag", "refs/tags/v1"), ("manual", "refs/heads/master")):
            self.assertFalse(vcr.save_allowed(event, ref), (event, ref))


class CacheReportCliTests(_Tmp):
    def run_cli(self, *args: str) -> list:
        proc = subprocess.run([sys.executable, "-I", str(SCRIPT), *args],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,  # argparse usage goes to stderr
                              stdin=subprocess.DEVNULL, env=neutral_env(self.tmp), timeout=60)
        self.assertEqual(proc.returncode, 0)
        return proc.stdout.decode("utf-8").splitlines()

    def test_SnapshotThenReportPrintsExactlyOneLine(self) -> None:
        cache = self.archives(2)
        self.assertEqual(self.run_cli("--cache-dir", str(cache), "snapshot"), [])
        lines = self.run_cli("--cache-dir", str(cache), "report", "--log",
                             str(self.log(LOG_RESTORED)))
        self.assertEqual(lines, ["info: vcpkg cache exact, 2 archives, 0.0 MB, "
                                 "0 packages built from source"])

    def test_BadUsageStillExits0WithOneLine(self) -> None:
        lines = self.run_cli("report")
        self.assertEqual(len(lines), 1, lines)
        self.assertTrue(lines[0].startswith("info: vcpkg cache report unavailable"), lines)


if __name__ == "__main__":
    unittest.main()
