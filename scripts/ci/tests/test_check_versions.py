"""Tests for ``check_versions.py`` (milestone 0014 plan SC3; ADR 0005 D5; Story 03.0 Task 01).

Classification: unit tests on temporary repository trees (a root
``CMakeLists.txt``, ``smbios_c/CMakeLists.txt`` and a sites file), plus one
check of the real repository. No network, no subprocess except the CLI test.

On failure, first check: ``scripts/ci/version_sites.json`` and the
``set(SMBIOS_C_VERSION ...)`` line of ``smbios_c/CMakeLists.txt``; the real
repository passes only while every listed site agrees (interim authority
until milestone 0012 adds the root ``project()`` VERSION).
"""

from __future__ import annotations

import contextlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import List, Optional, Tuple

from ci_test_support import CI_DIR, neutral_env

import check_versions as cv  # noqa: E402
from reporters import get_reporter  # noqa: E402

REPO_ROOT = CI_DIR.parents[1]


class _Tree(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        (self.root / "smbios_c").mkdir()
        self.sites = self.root / "sites.json"
        self.write_sites([{"site": "SMBIOS_C_VERSION", "file": "smbios_c/CMakeLists.txt",
                           "extractor": "cmake_set:SMBIOS_C_VERSION", "rule": "derived"}])

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def write_sites(self, sites: List[dict], interim: Optional[str] = "SMBIOS_C_VERSION") -> None:
        data = {"authoritative": {"site": "root project() VERSION", "file": "CMakeLists.txt",
                                  "extractor": "cmake_project_version"},
                "sites": sites}
        if interim is not None:
            data["interim_authoritative"] = interim
        self.sites.write_text(json.dumps(data), encoding="utf-8")

    def files(self, root_cmake: str, c_cmake: str) -> None:
        (self.root / "CMakeLists.txt").write_text(root_cmake, encoding="utf-8")
        (self.root / "smbios_c" / "CMakeLists.txt").write_text(c_cmake, encoding="utf-8")

    def run_check(self, tag: Optional[str] = None, backend: str = "plain") -> Tuple[int, List[str]]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            rc = cv.run(self.root, sites_file=self.sites, tag=tag,
                        reporter=get_reporter(backend), results=self.root / "results.jsonl")
        return rc, out.getvalue().splitlines()


class InterimAuthorityTests(_Tree):
    """Today's state: no root VERSION, SMBIOS_C_VERSION stands in (plan SC3 prerequisite)."""

    def test_SingleSitePassesTrivially(self) -> None:
        self.files("project(smbios_utility CXX)\n", 'set(SMBIOS_C_VERSION "1.0.0")\n')
        rc, lines = self.run_check()
        self.assertEqual(rc, 0, lines)
        self.assertRegex(lines[0], r"^ok: versions: 1\.0\.0 \(SMBIOS_C_VERSION, interim\), "
                                   r"1 sites \(")

    def test_SecondSiteMismatchIsADiscrepancy(self) -> None:
        self.write_sites([{"site": "SMBIOS_C_VERSION", "file": "smbios_c/CMakeLists.txt",
                           "extractor": "cmake_set:SMBIOS_C_VERSION", "rule": "derived"},
                          {"site": "port vcpkg.json", "file": "port.json",
                           "extractor": "json_key:version", "rule": "equal"}])
        self.files("project(x)\n", 'set(SMBIOS_C_VERSION "1.0.0")\n')
        (self.root / "port.json").write_text('{"version": "1.0.1"}', encoding="utf-8")
        rc, lines = self.run_check()
        self.assertEqual(rc, 1)
        self.assertIn("port vcpkg.json: expected 1.0.0 -> actual 1.0.1", lines[0])

    def test_MissingSiteIsADiscrepancy(self) -> None:
        self.files("project(x)\n", "# nothing here\n")
        rc, lines = self.run_check()
        self.assertEqual(rc, 1)
        self.assertIn("SMBIOS_C_VERSION (authoritative): expected X.Y.Z -> actual missing",
                      lines[0])

    def test_CommentedOutValueIsIgnored(self) -> None:
        self.files("project(x)\n", '# set(SMBIOS_C_VERSION "9.9.9")\n'
                                   '#[[ set(SMBIOS_C_VERSION "8.8.8") ]]\n'
                                   'set(SMBIOS_C_VERSION "1.2.3") # trailing\n')
        rc, lines = self.run_check()
        self.assertEqual(rc, 0, lines)
        self.assertIn("1.2.3", lines[0])

    def test_HostileValueIsNotEchoed(self) -> None:
        self.write_sites([{"site": "SMBIOS_C_VERSION", "file": "smbios_c/CMakeLists.txt",
                           "extractor": "cmake_set:SMBIOS_C_VERSION", "rule": "derived"},
                          {"site": "other", "file": "smbios_c/CMakeLists.txt",
                           "extractor": "cmake_set:OTHER", "rule": "equal"}])
        self.files("project(x)\n", 'set(SMBIOS_C_VERSION "1.0.0")\n'
                                   "set(OTHER ::error::x##teamcity[y])\n")
        rc, lines = self.run_check(backend="github")
        self.assertEqual(rc, 1)
        text = "\n".join(lines)
        self.assertIn("other: expected 1.0.0 -> actual <invalid>", text)
        self.assertNotIn("::error::x", text)
        self.assertNotIn("##teamcity[", text)


class RootAuthorityTests(_Tree):
    """After milestone 0012: the root project() VERSION is authoritative (ADR 0005 D5)."""

    def test_DerivedSiteReferencingProjectVersionPasses(self) -> None:
        self.files("project(smbios_utility VERSION 2.1.0 LANGUAGES CXX)\n",
                   "set(SMBIOS_C_VERSION ${PROJECT_VERSION})\n")
        rc, lines = self.run_check()
        self.assertEqual(rc, 0, lines)
        self.assertIn("2.1.0 (root project() VERSION), 1 sites", lines[0])

    def test_HandEditedLiteralInADerivedSiteFails(self) -> None:
        self.files("project(smbios_utility VERSION 2.1.0)\n", 'set(SMBIOS_C_VERSION "2.1.0")\n')
        rc, lines = self.run_check()
        self.assertEqual(rc, 1)
        self.assertIn("SMBIOS_C_VERSION: expected ${PROJECT_VERSION} -> actual 2.1.0", lines[0])

    def test_OtherVariableFails(self) -> None:
        self.files("project(p VERSION 2.1.0)\n", "set(SMBIOS_C_VERSION ${FOO})\n")
        rc, lines = self.run_check()
        self.assertEqual(rc, 1)
        self.assertIn("actual another variable", lines[0])

    def test_EqualSiteMismatchAndPlusMore(self) -> None:
        sites = [{"site": f"s{i}", "file": "smbios_c/CMakeLists.txt",
                  "extractor": f"cmake_set:V{i}", "rule": "equal"} for i in range(25)]
        self.write_sites(sites, interim=None)
        self.files("project(p VERSION 3.0.0)\n",
                   "".join(f'set(V{i} "1.{i}.0")\n' for i in range(25)))
        rc, lines = self.run_check()
        self.assertEqual(rc, 1)
        self.assertIn("s0: expected 3.0.0 -> actual 1.0.0", lines[0])
        self.assertIn("(+5 more)", lines)


class TagTests(_Tree):
    """``--tag``: v* tags must equal the authoritative version; other tags are not releases."""

    def setUp(self) -> None:
        super().setUp()
        self.files("project(p VERSION 1.4.2)\n", "set(SMBIOS_C_VERSION ${PROJECT_VERSION})\n")

    def test_MatchingAndPrereleaseTagsPass(self) -> None:
        for tag in ("v1.4.2", "v1.4.2-rc.1"):
            with self.subTest(tag=tag):
                rc, lines = self.run_check(tag=tag)
                self.assertEqual(rc, 0, lines)
                self.assertIn(f"tag {tag} ok", lines[0])

    def test_MismatchedOrMalformedTagFails(self) -> None:
        for tag in ("v1.4.3", "v1.4", "v1.4.2.1", "vX"):
            with self.subTest(tag=tag):
                rc, lines = self.run_check(tag=tag)
                self.assertEqual(rc, 1)
                self.assertIn("tag: expected v1.4.2 -> actual", lines[0])

    def test_LegacyGitTagIsNotARelease(self) -> None:
        rc, lines = self.run_check(tag="1.0.2.202611131")
        self.assertEqual(rc, 0)
        self.assertIn("not a release", lines[0])


class DataFileTests(_Tree):
    """An invalid sites file is an environment error (exit 2), never a pass."""

    def test_UnknownExtractorOrRuleExits2(self) -> None:
        self.files("project(x)\n", 'set(SMBIOS_C_VERSION "1.0.0")\n')
        for site in ({"site": "a", "file": "f", "extractor": "regex:.*"},
                     {"site": "a", "file": "f", "extractor": "cmake_set:A", "rule": "close"},
                     {"site": "a", "file": "f", "extractor": "cmake_set:A B"}):
            with self.subTest(site=site):
                self.write_sites([{"site": "SMBIOS_C_VERSION", "file": "smbios_c/CMakeLists.txt",
                                   "extractor": "cmake_set:SMBIOS_C_VERSION"}, site])
                rc, _ = self.run_check()
                self.assertEqual(rc, 2)

    def test_UnreadableOrMalformedFileExits2(self) -> None:
        self.sites.write_text("{", encoding="utf-8")
        self.assertEqual(self.run_check()[0], 2)
        self.sites.write_text('{"sites": {}}', encoding="utf-8")
        self.assertEqual(self.run_check()[0], 2)
        self.write_sites([], interim="NOPE")
        self.assertEqual(self.run_check()[0], 2)


class RealRepositoryTests(unittest.TestCase):
    """The shipped sites file against this repository (what CI's lint stage runs)."""

    def test_RepositoryIsConsistent(self) -> None:
        outcome = cv.check(REPO_ROOT)
        self.assertEqual(outcome.problems, [])
        self.assertRegex(outcome.authority or "", cv.VERSION.pattern)

    def test_CommandLine(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_name:
            proc = subprocess.run([sys.executable, "-I", str(CI_DIR / "check_versions.py"),
                                   "--tag", "build-123"], stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, env=neutral_env(Path(tmp_name)),
                                  timeout=60, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertRegex(proc.stdout, r"^ok: versions: .*, tag build-123 not a release \(")


if __name__ == "__main__":
    unittest.main()
