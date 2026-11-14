"""Tests for the pinned vcpkg bootstrap (milestone 0014, Story 02.0 Task 02; threat model CR-2, CR-7).

Covers ``scripts/ci/bootstrap_vcpkg.py``: the three-way baseline agreement,
the fetch-by-commit checkout and its ``HEAD`` check, the tool binary's
SHA-512 check, the exported environment (``VCPKG_ROOT`` is the bootstrapped
directory even when the image's ``VCPKG_INSTALLATION_ROOT`` is set, and a
``vcpkg`` on ``PATH`` is never used), the single ``ok:`` line, and the
``VCPKG_BINARY_SOURCES`` cache rules.

Classification: unit (pure functions) and mock (``main`` in-process against a
stub ``git`` on a temporary ``PATH``; the stub checkout writes a stub
``bootstrap-vcpkg.sh`` and metadata file). No network. POSIX only for the
stub runs (the stubs are ``/bin/sh`` trampolines).
On failure first check: the ``STUB_*`` switches in ``_run``; then
``bootstrap_vcpkg.expected_tool_sha512``'s platform key choice.
Real-runner behaviour (the actual fetch, ``bootstrap-vcpkg.bat`` on Windows,
the real tool hash) is verified in Story 03.0, not here.

Run: ``python3 scripts/ci/run_tests.py``.
"""

from __future__ import annotations

import contextlib
import hashlib
import io
import json
import os
import re
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Dict, List, Tuple
from unittest import mock

from ci_test_support import CI_DIR, write_stub

import bootstrap_vcpkg  # noqa: E402  (ci_test_support put scripts/ci on sys.path)

REPO_ROOT = CI_DIR.parents[1]
POSIX_ONLY = "stub commands are /bin/sh trampolines"
PINNED = json.loads((CI_DIR / "pins.json").read_text(encoding="utf-8"))["vcpkg"]["commit"]

#: What the stub bootstrap writes as the "vcpkg" tool binary.
#: The real tool prints the release date plus its 40-hex commit (checked on macOS
#: 2026-11-13); the ok: line keeps 12 hex so the token fits the 40-char cap.
TOOL_BODY = ("#!/bin/sh\necho 'vcpkg package management program version "
             "2026-07-27-deadbeef98d7cb0cf1f4686a3e43aa5672b6230c'\n")
TOOL_SHA512 = hashlib.sha512(TOOL_BODY.encode("ascii")).hexdigest()

GIT_STUB = """\
import os, stat, sys
args = sys.argv[1:]
with open(os.environ['STUB_CALLS'], 'a') as fh:
    fh.write(' '.join(args) + '\\n')
dest = args[1] if args[:1] == ['-C'] else '.'
sub = args[2] if args[:1] == ['-C'] else args[0]
if sub == 'fetch':
    sys.exit(int(os.environ.get('STUB_FETCH_RC', '0')))
if sub == 'checkout':
    os.makedirs(os.path.join(dest, 'scripts'), exist_ok=True)
    sha = os.environ['STUB_TOOL_SHA']
    with open(os.path.join(dest, 'scripts', 'vcpkg-tool-metadata.txt'), 'w') as fh:
        fh.write('VCPKG_TOOL_RELEASE_TAG=2026-07-27\\n')
        for key in ('MACOS', 'MUSLC', 'GLIBC', 'GLIBC_ARM64'):
            fh.write('VCPKG_%s_SHA=%s\\n' % (key, sha))
    path = os.path.join(dest, 'bootstrap-vcpkg.sh')
    with open(path, 'w') as fh:
        fh.write('#!/bin/sh\\nexec "%s" "%s" "$(dirname "$0")" "$@"\\n'
                 % (sys.executable, os.environ['STUB_BOOTSTRAP']))
    os.chmod(path, 0o755)
if sub == 'rev-parse':
    print(os.environ['STUB_HEAD'])
"""
BOOTSTRAP_STUB = """\
import os, sys
dest, args = sys.argv[1], sys.argv[2:]
if args != ['-disableMetrics']:
    print('error: expected -disableMetrics, got %r' % args)
    sys.exit(3)
path = os.path.join(dest, 'vcpkg')
with open(path, 'w', newline='\\n') as fh:
    fh.write(TOOL_BODY_PLACEHOLDER)
os.chmod(path, 0o755)
""".replace("TOOL_BODY_PLACEHOLDER", repr(TOOL_BODY))
EVIL_VCPKG = "print('vcpkg package management program version EVIL-PATH-LOOKUP')\n"


class BinarySourcesTests(unittest.TestCase):
    """Unit: the CR-2 cache rules."""

    D = Path("/tmp/cache")

    def test_TagOnPackagingLegIsExactlyClear(self) -> None:
        self.assertEqual(bootstrap_vcpkg.binary_sources("tag", "refs/tags/v1.0.0", True, self.D),
                         "clear")

    def test_PullRequestRestoresOnly(self) -> None:
        self.assertEqual(bootstrap_vcpkg.binary_sources("pr", "refs/pull/7/merge", False, self.D),
                         f"clear;files,{self.D},read")

    def test_MasterPushAndScheduleWrite(self) -> None:
        for event in ("push", "schedule"):
            self.assertEqual(bootstrap_vcpkg.binary_sources(event, "refs/heads/master", False,
                                                            self.D),
                             f"clear;files,{self.D},readwrite")

    def test_EveryOtherRunIsReadOnlyAndEveryResultStartsClear(self) -> None:
        cases = [("push", "refs/heads/feature", False), ("manual", "refs/heads/master", False),
                 ("tag", "refs/tags/v1", False), ("pr", "refs/heads/master", True),
                 ("schedule", "refs/heads/other", True), ("", "", False)]
        for event, ref, packaging in cases:
            with self.subTest(event=event, ref=ref):
                value = bootstrap_vcpkg.binary_sources(event, ref, packaging, self.D)
                self.assertTrue(value.startswith("clear;"), value)
                self.assertTrue(value.endswith(",read"), value)

    def test_CacheDirWithSyntaxCharactersRejected(self) -> None:
        for bad in ("/a,b", "/a;b", "/a`b"):
            with self.assertRaises(ValueError):
                bootstrap_vcpkg.binary_sources("pr", "x", False, Path(bad))


class BaselineTests(unittest.TestCase):
    """Unit: the three pin sites must agree."""

    def setUp(self) -> None:
        self._td = tempfile.TemporaryDirectory()
        self.addCleanup(self._td.cleanup)
        self.root = Path(self._td.name)

    def write(self, manifest: str, config: str, pinned: str) -> Path:
        (self.root / "vcpkg.json").write_text(json.dumps({"builtin-baseline": manifest}))
        (self.root / "vcpkg-configuration.json").write_text(
            json.dumps({"default-registry": {"baseline": config}}))
        pins = self.root / "pins.json"
        pins.write_text(json.dumps({"vcpkg": {"commit": pinned}}))
        return pins

    def test_RepositoryPinsAgree(self) -> None:
        self.assertEqual(bootstrap_vcpkg.pinned_baseline(REPO_ROOT, CI_DIR / "pins.json"), PINNED)

    def test_EachMismatchIsDiscrepancy(self) -> None:
        a, b = "a" * 40, "b" * 40
        for manifest, config, pinned in ((a, b, a), (a, a, b)):
            pins = self.write(manifest, config, pinned)
            with self.assertRaises(bootstrap_vcpkg.BootstrapError) as ctx:
                bootstrap_vcpkg.pinned_baseline(self.root, pins)
            self.assertEqual(ctx.exception.message, f"vcpkg baseline: expected {a} -> actual {b}")

    def test_NonHexValueRejected(self) -> None:
        pins = self.write("A" * 40, "A" * 40, "A" * 40)
        with self.assertRaises(bootstrap_vcpkg.BootstrapError):
            bootstrap_vcpkg.pinned_baseline(self.root, pins)


class ToolHashSelectionTests(unittest.TestCase):
    """Unit: which SHA-512 the tool must have, per platform."""

    def setUp(self) -> None:
        self._td = tempfile.TemporaryDirectory()
        self.addCleanup(self._td.cleanup)
        self.dest = Path(self._td.name)
        (self.dest / "scripts").mkdir()
        (self.dest / "scripts" / "vcpkg-tool-metadata.txt").write_text(
            "VCPKG_TOOL_RELEASE_TAG=2026-07-27\nVCPKG_MACOS_SHA=" + "1" * 128
            + "\nVCPKG_GLIBC_SHA=" + "2" * 128 + "\nVCPKG_GLIBC_ARM64_SHA=" + "3" * 128
            + "\nVCPKG_MUSLC_SHA=" + "4" * 128 + "\n")
        self.pins = {"vcpkg": {"tool_release_tag": "2026-07-27",
                               "windows_tool_sha512": {"x64": "5" * 128, "arm64": "6" * 128}}}

    def pick(self, system: str, machine: str, musl: bool = False) -> str:
        return bootstrap_vcpkg.expected_tool_sha512(self.dest, self.pins, system=system,
                                                    machine=machine, musl=musl)

    def test_PlatformKeys(self) -> None:
        self.assertEqual(self.pick("Darwin", "arm64"), "1" * 128)
        self.assertEqual(self.pick("Linux", "x86_64"), "2" * 128)
        self.assertEqual(self.pick("Linux", "aarch64"), "3" * 128)
        self.assertEqual(self.pick("Linux", "x86_64", musl=True), "4" * 128)
        self.assertEqual(self.pick("Windows", "AMD64"), "5" * 128)
        self.assertEqual(self.pick("Windows", "ARM64"), "6" * 128)

    def test_WindowsPinMustMatchMetadataRelease(self) -> None:
        self.pins["vcpkg"]["tool_release_tag"] = "2025-01-01"
        with self.assertRaises(bootstrap_vcpkg.BootstrapError) as ctx:
            self.pick("Windows", "AMD64")
        self.assertIn("vcpkg tool release: expected 2025-01-01 -> actual 2026-07-27",
                      ctx.exception.message)

    def test_UnsupportedPlatformFailsClosed(self) -> None:
        with self.assertRaises(bootstrap_vcpkg.BootstrapError):
            self.pick("FreeBSD", "amd64")

    def test_RepositoryPinsCarryWindowsHashes(self) -> None:
        pins = json.loads((CI_DIR / "pins.json").read_text(encoding="utf-8"))["vcpkg"]
        self.assertRegex(pins["tool_release_tag"], r"^\d{4}-\d{2}-\d{2}$")
        for arch in ("x64", "arm64"):
            self.assertRegex(pins["windows_tool_sha512"][arch], r"^[0-9a-f]{128}$")


@unittest.skipIf(os.name == "nt", POSIX_ONLY)
class BootstrapRunTests(unittest.TestCase):
    """Mock: ``main`` in-process with a stub git and stub bootstrap."""

    def setUp(self) -> None:
        self._td = tempfile.TemporaryDirectory(prefix="ci-vcpkg-test-")
        self.addCleanup(self._td.cleanup)
        self.tmp = Path(self._td.name).resolve()
        self.stubs = self.tmp / "bin"
        write_stub(self.stubs, "git", GIT_STUB)
        write_stub(self.stubs, "vcpkg", EVIL_VCPKG)  # must never be used
        self.bootstrap_stub = self.tmp / "bootstrap_stub.py"
        self.bootstrap_stub.write_text(BOOTSTRAP_STUB, encoding="utf-8")
        self.calls = self.tmp / "git-calls.txt"
        self.image_root = self.tmp / "image-vcpkg"

    def _run(self, argv: List[str] = (), **stub: str) -> Tuple[int, List[str], Dict[str, str]]:
        env = {k: v for k, v in os.environ.items() if not k.startswith(("CI_", "VCPKG_"))}
        env.update({
            "PATH": str(self.stubs) + os.pathsep + os.environ.get("PATH", ""),
            "CI_TEMP_DIR": str(self.tmp / "temp"), "CI_LOG_DIR": str(self.tmp / "logs"),
            "CI_RESULTS_FILE": str(self.tmp / "results.jsonl"), "CI_REPORTER": "plain",
            "CI_EVENT": "pr", "CI_REF": "refs/pull/1/merge",
            "VCPKG_INSTALLATION_ROOT": str(self.image_root),
            "STUB_CALLS": str(self.calls), "STUB_BOOTSTRAP": str(self.bootstrap_stub),
            "STUB_HEAD": PINNED, "STUB_TOOL_SHA": TOOL_SHA512,
        })
        env.update(stub)
        out = io.StringIO()
        with mock.patch.dict(os.environ, env, clear=True), contextlib.redirect_stdout(out):
            rc = bootstrap_vcpkg.main(["--repo-root", str(REPO_ROOT), *argv])
            exported = {k: os.environ.get(k, "") for k in
                        ("VCPKG_ROOT", "VCPKG_DISABLE_METRICS", "VCPKG_BINARY_SOURCES")}
        return rc, out.getvalue().splitlines(), exported

    def test_SuccessExportsBootstrappedRootAndPrintsOneLine(self) -> None:
        rc, lines, exported = self._run()
        self.assertEqual(rc, 0, lines)
        self.assertEqual(len(lines), 1, lines)
        self.assertRegex(lines[0], rf"^ok: vcpkg bootstrap: {PINNED[:12]}, tool "
                                   r"2026-07-27-deadbeef98d7 \(\d+\.\ds\)$")
        root = Path(exported["VCPKG_ROOT"])
        self.assertNotEqual(root, self.image_root)
        self.assertEqual(root.parent, self.tmp / "temp")
        self.assertTrue((root / "vcpkg").is_file())
        self.assertEqual(exported["VCPKG_DISABLE_METRICS"], "1")
        self.assertEqual(exported["VCPKG_BINARY_SOURCES"],
                         f"clear;files,{self.tmp / 'temp' / 'vcpkg-binary-cache'},read")
        calls = self.calls.read_text().splitlines()
        self.assertIn(f"-C {root} fetch -q --depth 1 {bootstrap_vcpkg.VCPKG_URL} {PINNED}", calls)
        self.assertIn(f"-C {root} checkout -q --detach FETCH_HEAD", calls)

    def test_TagPackagingLegIsCold(self) -> None:
        rc, lines, exported = self._run(["--packaging-leg"], CI_EVENT="tag",
                                        CI_REF="refs/tags/v1.0.0")
        self.assertEqual(rc, 0, lines)
        self.assertEqual(exported["VCPKG_BINARY_SOURCES"], "clear")

    def test_BaselineMismatchExits1(self) -> None:
        root = self.tmp / "repo"
        root.mkdir()
        (root / "vcpkg.json").write_text(json.dumps({"builtin-baseline": PINNED}))
        (root / "vcpkg-configuration.json").write_text(
            json.dumps({"default-registry": {"baseline": "c" * 40}}))
        rc, lines, _ = self._run(["--repo-root", str(root)])
        self.assertEqual(rc, 1, lines)
        self.assertTrue(lines[0].startswith("error: vcpkg bootstrap failed (exit 1"), lines)
        self.assertIn(f"vcpkg baseline: expected {PINNED} -> actual {'c' * 40}", lines[0])
        self.assertFalse(self.calls.exists(), "git must not run after a baseline mismatch")

    def test_HeadMismatchExits1(self) -> None:
        rc, lines, exported = self._run(STUB_HEAD="d" * 40)
        self.assertEqual(rc, 1, lines)
        self.assertIn(f"vcpkg HEAD: expected {PINNED} -> actual {'d' * 40}", lines[0])
        self.assertEqual(exported["VCPKG_ROOT"], "")

    def test_ToolHashMismatchExits1(self) -> None:
        rc, lines, exported = self._run(STUB_TOOL_SHA="0" * 128)
        self.assertEqual(rc, 1, lines)
        self.assertIn("vcpkg tool sha512: expected 0000000000000000 -> actual "
                      + TOOL_SHA512[:16], lines[0])
        self.assertEqual(exported["VCPKG_ROOT"], "")

    def test_FetchFailurePrintsRc1(self) -> None:
        rc, lines, _ = self._run(STUB_FETCH_RC="128")
        self.assertEqual(rc, 1, lines)
        self.assertTrue(lines[0].startswith("error: vcpkg git fetch failed (exit 128"), lines)


if __name__ == "__main__":
    unittest.main()
