"""Tests for ``gui_guards.py`` (milestone 0014, Story 03.0 Task 03; 0011 R-7/R-8, threat model CR-12).

Classification: unit tests of the pure helpers (occurrence counting, ``Total
Tests:`` parsing, the build-tree walk) plus command-line tests of
``gui_guards.py`` in a subprocess against fake binaries and a stub ``ctest``
on a temporary ``PATH``. No network, no host cmake/ctest, no real GUI build;
``smbios_gui`` is never executed.

Scenario coverage, per guard: a passing case, a failing case (hook string
present; local tier present) and every fail-closed path (unreadable or
non-regular binary; ctest missing, failing, timing out, printing no / two /
a non-numeric ``Total Tests:`` line; a missing or unlistable build tree).
"Negative proof" cases inject the forbidden string into a copy of a fake
binary (and of the running Python interpreter, a real executable) and show
the guard turns red, so a guard that silently passes cannot go unnoticed.

On failure, first check: ``HOOK_PATTERNS``, ``LOCAL_PREFIXES`` and
``_TOTAL_TESTS`` in ``gui_guards.py`` against the old ``ci.yml`` gui-build
bash (``grep -a -c -F -e``, ``count_total``, ``find`` without ``-type f``) -
the semantics must stay identical or stronger, never weaker (merge-blocking).
"""

from __future__ import annotations

import contextlib
import io
import os
import random
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import List, Optional
from unittest import mock

from ci_test_support import CI_DIR, neutral_env, path_with, write_stub

import gui_guards as gg  # noqa: E402
from reporters import get_reporter  # noqa: E402

SCRIPT = CI_DIR / "gui_guards.py"
POSIX_ONLY = "stub commands are /bin/sh trampolines (POSIX only)"
IS_ROOT = hasattr(os, "geteuid") and os.geteuid() == 0
HOOK = b"--exit-after-frames"
USAGE = b"usage: --exit-after-frames"
#: A plausible fake shipped binary: header, padding and unrelated option strings;
#: above the R-7 size floor (``gg.MIN_BINARY_BYTES``, review S3-2).
FAKE_BINARY = (b"\x7fELF\x02\x01\x01" + b"\0" * 57 + b"--help\0--version\0smbios_gui\0"
               + bytes(range(256)) * 260 + b"--exit-after\0-frames\0")


def _py(code: str) -> List[str]:
    return [sys.executable, "-c", code]


class _Captured:
    """Run a check in-process with the plain reporter; collect lines and records."""

    def __init__(self, tmp: Path) -> None:
        self.ctx = gg.Ctx(get_reporter("plain"), tmp / "results.jsonl")

    def __call__(self, fn, *args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = fn(*args, self.ctx)
        return rc, out.getvalue().splitlines()


class CountOccurrencesTests(unittest.TestCase):
    """``count_occurrences`` equals ``bytes.count`` over the whole file, whatever the chunking."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def write(self, data: bytes, name: str = "bin") -> Path:
        path = self.tmp / name
        path.write_bytes(data)
        return path

    def test_ZeroOneThreeOccurrences(self) -> None:
        for n in (0, 1, 3):
            with self.subTest(n=n):
                path = self.write(FAKE_BINARY + (b"\0" + HOOK) * n + b"\0tail")
                self.assertEqual(gg.count_occurrences(path, HOOK), n)
                self.assertEqual(gg.count_occurrences(path, USAGE), 0)

    def test_PatternStraddlingEveryChunkBoundaryIsCountedOnce(self) -> None:
        for chunk in range(1, len(USAGE) + 3):
            for offset in range(0, chunk + 1):
                with self.subTest(chunk=chunk, offset=offset):
                    data = b"x" * offset + USAGE + b"y" * 5
                    path = self.write(data)
                    self.assertEqual(gg.count_occurrences(path, USAGE, chunk), 1)
                    self.assertEqual(gg.count_occurrences(path, HOOK, chunk), 1)

    def test_RealChunkSizeBoundary(self) -> None:
        cut = gg.CHUNK - len(HOOK) // 2
        path = self.write(b"\0" * cut + HOOK + b"\0" * 100)
        self.assertEqual(gg.count_occurrences(path, HOOK), 1)

    def test_SelfOverlappingPatternIsNonOverlapping(self) -> None:
        for chunk in range(1, 8):
            with self.subTest(chunk=chunk):
                path = self.write(b"aaaaa")
                self.assertEqual(gg.count_occurrences(path, b"aa", chunk), b"aaaaa".count(b"aa"))

    def test_RandomisedAgainstBytesCount(self) -> None:
        rng = random.Random(20261113)
        for _ in range(300):
            alphabet = b"-ab"
            pattern = bytes(rng.choice(alphabet) for _ in range(rng.randint(1, 4)))
            data = bytes(rng.choice(alphabet) for _ in range(rng.randint(0, 60)))
            chunk = rng.randint(1, 10)
            path = self.write(data)
            self.assertEqual(gg.count_occurrences(path, pattern, chunk), data.count(pattern),
                             (pattern, data, chunk))

    def test_EmptyFileAndBadArguments(self) -> None:
        path = self.write(b"")
        self.assertEqual(gg.count_occurrences(path, HOOK), 0)
        with self.assertRaises(ValueError):
            gg.count_occurrences(path, b"")
        with self.assertRaises(ValueError):
            gg.count_occurrences(path, HOOK, 0)

    def test_NonRegularPathsRaise(self) -> None:
        with self.assertRaises(OSError):
            gg.count_occurrences(self.tmp / "missing", HOOK)
        with self.assertRaises(IsADirectoryError):
            gg.count_occurrences(self.tmp, HOOK)
        if os.name != "nt":
            target = self.write(FAKE_BINARY + HOOK, "target")
            link = self.tmp / "link"
            link.symlink_to(target)
            with self.assertRaises(OSError) as err:
                gg.count_occurrences(link, HOOK)
            self.assertEqual(gg.errno_name(err.exception), "ELOOP")


class HookStringsTests(unittest.TestCase):
    """R-7 / C18 through ``check_hook_strings`` (in-process) and the CLI."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        self.run_check = _Captured(self.tmp)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def binary(self, data: bytes, name: str = "smbios_gui") -> Path:
        path = self.tmp / name
        path.write_bytes(data)
        return path

    def test_CleanBinaryPasses(self) -> None:
        rc, lines = self.run_check(gg.check_hook_strings, [self.binary(FAKE_BINARY)])
        self.assertEqual(rc, 0)
        self.assertEqual(len(lines), 1)
        self.assertRegex(lines[0], r"^ok: no test hook strings in the shipped smbios_gui "
                                   r"\(\d+\.\ds\)$")

    def test_EachPatternZeroOneThree(self) -> None:
        for pattern in (HOOK, USAGE):
            for n in (0, 1, 3):
                with self.subTest(pattern=pattern, n=n):
                    data = FAKE_BINARY + (b"\n" + pattern + b"\n") * n
                    rc, lines = self.run_check(gg.check_hook_strings, [self.binary(data)])
                    self.assertEqual(rc, 0 if n == 0 else 1, lines)
                    if n:
                        self.assertIn(f"test hook present in shipped smbios_gui: expected 0 -> {n}",
                                      lines[0])
                        usage_n = n if pattern == USAGE else 0
                        self.assertIn(f"'--exit-after-frames': expected 0 -> {n}", lines)
                        self.assertIn(f"'usage: --exit-after-frames': expected 0 -> {usage_n}",
                                      lines)

    def test_OccurrencesOnOneLineCountEach(self) -> None:
        # grep -c would print 1 (one line); this counts 3 occurrences. Both fail the gate.
        rc, lines = self.run_check(gg.check_hook_strings,
                                   [self.binary(FAKE_BINARY + HOOK + HOOK + b" " + HOOK)])
        self.assertEqual(rc, 1)
        self.assertIn("expected 0 -> 3", lines[0])

    def test_NegativeProofInjectedIntoFakeBinaryCopy(self) -> None:
        original = self.binary(FAKE_BINARY, "original")
        self.assertEqual(self.run_check(gg.check_hook_strings, [original])[0], 0)
        for where in (0, len(FAKE_BINARY) // 2, len(FAKE_BINARY)):
            with self.subTest(where=where):
                data = original.read_bytes()
                copy = self.binary(data[:where] + USAGE + data[where:], "copy")
                rc, lines = self.run_check(gg.check_hook_strings, [copy])
                self.assertEqual(rc, 1, "the guard must turn red on an injected hook string")
                self.assertIn("test hook present in shipped smbios_gui: expected 0 -> 1",
                              lines[0])

    def test_NegativeProofInjectedIntoARealExecutableCopy(self) -> None:
        real = Path(sys.executable).resolve()
        clean = self.tmp / "real"
        shutil.copyfile(real, clean)
        # A shared-libpython interpreter can be a few KiB: pad it past the R-7
        # size floor (S3-2) so this case still proves the hook-string count.
        short = gg.MIN_BINARY_BYTES - clean.stat().st_size
        if short > 0:
            with clean.open("ab") as fh:
                fh.write(b"\0" * short)
        self.assertEqual(self.run_check(gg.check_hook_strings, [clean])[0], 0,
                         "the interpreter itself holds no hook string")
        data = clean.read_bytes()
        cut = min(len(data), gg.CHUNK) - 3  # across the first chunk boundary when large enough
        injected = self.binary(data[:cut] + HOOK + data[cut:], "real-injected")
        rc, lines = self.run_check(gg.check_hook_strings, [injected])
        self.assertEqual(rc, 1)
        self.assertIn("expected 0 -> 1", lines[0])

    def test_AnyOfSeveralBinariesFails(self) -> None:
        clean = self.binary(FAKE_BINARY, "a")
        dirty = self.binary(FAKE_BINARY + HOOK, "b")
        self.assertEqual(self.run_check(gg.check_hook_strings, [clean, dirty])[0], 1)

    def test_FakeBinaryIsAboveTheSizeFloor(self) -> None:
        self.assertGreaterEqual(len(FAKE_BINARY), gg.MIN_BINARY_BYTES)

    def test_EmptyOrTruncatedBinaryFailsClosed(self) -> None:
        """S3-2 regression: a 0-byte smbios_gui used to print ``ok:`` (0 occurrences)."""
        floor = gg.MIN_BINARY_BYTES
        for size in (0, 1, floor - 1):
            with self.subTest(size=size):
                rc, lines = self.run_check(gg.check_hook_strings,
                                           [self.binary(FAKE_BINARY[:size])])
                self.assertEqual(rc, 1, lines)
                self.assertIn(f"smbios_gui binary size: expected >= {floor} -> {size}", lines[0])
                self.assertFalse(any(x.startswith("ok:") for x in lines), lines)

    def test_BinaryAtTheSizeFloorPasses(self) -> None:
        rc, lines = self.run_check(gg.check_hook_strings,
                                   [self.binary(FAKE_BINARY[:gg.MIN_BINARY_BYTES])])
        self.assertEqual(rc, 0, lines)

    def test_SmallBinaryWithHookReportsTheHookFirst(self) -> None:
        rc, lines = self.run_check(gg.check_hook_strings, [self.binary(HOOK)])
        self.assertEqual(rc, 1)
        self.assertIn("test hook present in shipped smbios_gui: expected 0 -> 1", lines[0])

    def test_AnyTruncatedOfSeveralBinariesFails(self) -> None:
        rc, lines = self.run_check(gg.check_hook_strings,
                                   [self.binary(FAKE_BINARY, "a"), self.binary(b"", "b")])
        self.assertEqual(rc, 1)
        self.assertIn("-> 0", lines[0])

    def test_CliEmptyBinaryExitsOne(self) -> None:
        empty = self.binary(b"")
        proc = subprocess.run([sys.executable, "-I", str(SCRIPT), "hook-strings", "--binary",
                               str(empty)], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              env=neutral_env(self.tmp), timeout=60, text=True)
        self.assertEqual(proc.returncode, 1, proc.stdout + proc.stderr)
        self.assertIn("smbios_gui binary size: expected >= ", proc.stdout)

    def test_NoBinaryFailsClosed(self) -> None:
        rc, lines = self.run_check(gg.check_hook_strings, [])
        self.assertEqual(rc, 1)
        self.assertIn("smbios_gui binary: expected 1 -> actual 0", lines[0])

    def test_MissingDirectoryAndSymlinkFailClosed(self) -> None:
        cases = {"ENOENT": self.tmp / "missing", "EISDIR": self.tmp}
        if os.name != "nt":
            link = self.tmp / "link"
            link.symlink_to(self.binary(FAKE_BINARY))
            cases["ELOOP"] = link
        for name, path in cases.items():
            with self.subTest(errno=name):
                rc, lines = self.run_check(gg.check_hook_strings, [path])
                self.assertEqual(rc, 1)
                self.assertIn("hook string check could not read the shipped smbios_gui "
                              f"({name})", lines[0])

    @unittest.skipIf(os.name == "nt" or IS_ROOT, "needs POSIX permissions and a non-root user")
    def test_UnreadableFileFailsClosed(self) -> None:
        path = self.binary(FAKE_BINARY)
        path.chmod(0)
        try:
            rc, lines = self.run_check(gg.check_hook_strings, [path])
        finally:
            path.chmod(stat.S_IRUSR | stat.S_IWUSR)
        self.assertEqual(rc, 1)
        self.assertIn("hook string check could not read the shipped smbios_gui (EACCES)", lines[0])

    @unittest.skipIf(not hasattr(os, "mkfifo"), "needs mkfifo")
    def test_FifoFailsClosedWithoutBlocking(self) -> None:
        fifo = self.tmp / "smbios_gui"
        os.mkfifo(fifo)
        # In a child with a time limit: a regression that opens the FIFO blocks,
        # and must fail this test (TimeoutExpired), not hang the suite.
        proc = subprocess.run([sys.executable, "-I", str(SCRIPT), "hook-strings", "--binary",
                               str(fifo)], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              env=neutral_env(self.tmp), timeout=30, text=True)
        self.assertEqual(proc.returncode, 1)
        self.assertIn("(EINVAL)", proc.stdout)

    def test_ReadErrorMidFileFailsClosed(self) -> None:
        path = self.binary(FAKE_BINARY)
        real_fdopen = os.fdopen

        class _Broken(io.RawIOBase):
            def __init__(self, fd: int) -> None:
                self._fh = real_fdopen(fd, "rb", buffering=0)

            def read(self, n: int = -1) -> bytes:  # noqa: D102
                raise OSError(5, "I/O error")

            def close(self) -> None:  # noqa: D102
                self._fh.close()
                super().close()

        with mock.patch("gui_guards.os.fdopen", lambda fd, *a, **k: _Broken(fd)):
            rc, lines = self.run_check(gg.check_hook_strings, [path])
        self.assertEqual(rc, 1)
        self.assertIn("(EIO)", lines[0])

    def test_CliExitCodes(self) -> None:
        clean = self.binary(FAKE_BINARY)
        dirty = self.binary(FAKE_BINARY + USAGE, "dirty")
        for path, want in ((clean, 0), (dirty, 1), (self.tmp / "missing", 1)):
            with self.subTest(path=path.name):
                proc = subprocess.run([sys.executable, "-I", str(SCRIPT), "--reporter", "github",
                                       "hook-strings", "--binary", str(path)],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                      env=neutral_env(self.tmp), timeout=60, text=True)
                self.assertEqual(proc.returncode, want, proc.stdout + proc.stderr)
                if want:
                    self.assertTrue(proc.stdout.startswith("::error title=gui hook strings::"))
        proc = subprocess.run([sys.executable, "-I", str(SCRIPT), "hook-strings"],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              env=neutral_env(self.tmp), timeout=60, text=True)
        self.assertEqual(proc.returncode, 2, "usage error")


class FindBinaryTests(unittest.TestCase):
    """C17: ``find -type f -path '*/smbios_gui/bin/*' -name smbios_gui[.exe]``."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        self.build = self.tmp / "build" / "linux-gui"
        self.build.mkdir(parents=True)
        self.run_check = _Captured(self.tmp)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def put(self, rel: str, data: bytes = FAKE_BINARY) -> Path:
        path = self.build / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def test_FoundRegularFile(self) -> None:
        path = self.put("smbios_gui/bin/smbios_gui")
        self.assertEqual(gg.find_binaries(self.build), [path])
        (rc, binary), lines = self.run_check(gg.check_find_binary, self.build)
        self.assertEqual((rc, binary), (0, path))
        self.assertEqual(len(lines), 1)

    def test_OkLineShape(self) -> None:
        self.put("smbios_gui/bin/smbios_gui")
        out = io.StringIO()
        old = os.getcwd()
        os.chdir(self.tmp)
        try:
            with contextlib.redirect_stdout(out):
                rc, binary = gg.check_find_binary(Path("build/linux-gui"), self.run_check.ctx)
        finally:
            os.chdir(old)
        self.assertEqual(rc, 0)
        self.assertEqual(binary, Path("build/linux-gui/smbios_gui/bin/smbios_gui"))
        self.assertRegex(out.getvalue().strip(),
                         rf"^ok: smbios_gui: build/linux-gui/smbios_gui/bin/smbios_gui "
                         rf"\({len(FAKE_BINARY)} bytes\) \(\d+\.\ds\)$")

    def test_MultiConfigSubdirectoryAndExeMatch(self) -> None:
        # find's -path '*' crosses '/', so bin/Debug/smbios_gui.exe matched the old search too.
        path = self.put("smbios_gui/bin/Debug/smbios_gui.exe")
        self.assertEqual(gg.find_binaries(self.build), [path])

    def test_NonMatchingPathsAreIgnored(self) -> None:
        self.put("bin/smbios_gui")                     # not under smbios_gui/bin
        self.put("smbios_gui/bin/smbios_gui_autoexit")  # other name
        self.put("smbios_gui/bin/SMBIOS_GUI")           # find -name is case-sensitive
        (self.build / "smbios_gui" / "bin" / "x" / "smbios_gui").mkdir(parents=True)  # a dir
        self.assertEqual(gg.find_binaries(self.build), [])

    def test_NoneFoundFails(self) -> None:
        for build in (self.build, self.tmp / "missing"):
            with self.subTest(build=build.name):
                out = io.StringIO()
                with contextlib.redirect_stdout(out):
                    rc, binary = gg.check_find_binary(build, self.run_check.ctx)
                self.assertEqual((rc, binary), (1, None))
                self.assertIn("smbios_gui binary: expected 1 -> actual 0", out.getvalue())

    @unittest.skipIf(os.name == "nt", "symlinks need privileges on Windows")
    def test_SymlinkIsNeitherFollowedNorReturned(self) -> None:
        real = self.put("elsewhere/smbios_gui")
        bindir = self.build / "smbios_gui" / "bin"
        bindir.mkdir(parents=True)
        (bindir / "smbios_gui").symlink_to(real)
        self.assertEqual(gg.find_binaries(self.build), [])
        other = self.tmp / "other" / "smbios_gui" / "bin"
        other.mkdir(parents=True)
        (other / "smbios_gui").write_bytes(FAKE_BINARY)
        (self.build / "linked").symlink_to(self.tmp / "other", target_is_directory=True)
        self.assertEqual(gg.find_binaries(self.build), [])

    @unittest.skipIf(os.name == "nt" or IS_ROOT, "needs POSIX permissions and a non-root user")
    def test_UnlistableSubtreeFailsClosed(self) -> None:
        """S3-4 regression: a binary hidden in an unlistable subtree used to be skipped."""
        self.put("smbios_gui/bin/smbios_gui")
        hidden = self.put("other/smbios_gui/bin/smbios_gui", FAKE_BINARY + HOOK)
        locked = hidden.parents[2]
        locked.chmod(0)
        try:
            with self.assertRaises(PermissionError):
                gg.find_binaries(self.build)
            (rc, binary), lines = self.run_check(gg.check_find_binary, self.build)
            hrc, hlines = self.run_check(gg.check_hook_strings_in_tree, self.build)
        finally:
            locked.chmod(stat.S_IRWXU)
        self.assertEqual((rc, binary), (1, None))
        self.assertIn("smbios_gui binary search failed (EACCES)", lines[0])
        self.assertEqual(hrc, 1)
        self.assertIn("smbios_gui binary search failed (EACCES)", hlines[0])
        self.assertFalse(any(x.startswith("ok:") for x in lines + hlines))

    def test_ReadableTreeInTreeCheckPassesAndFindsEveryBinary(self) -> None:
        self.put("smbios_gui/bin/smbios_gui")
        rc, lines = self.run_check(gg.check_hook_strings_in_tree, self.build)
        self.assertEqual(rc, 0, lines)
        self.put("other/smbios_gui/bin/smbios_gui", FAKE_BINARY + HOOK)
        rc, lines = self.run_check(gg.check_hook_strings_in_tree, self.build)
        self.assertEqual(rc, 1, lines)
        rc, lines = self.run_check(gg.check_hook_strings_in_tree, self.tmp / "missing")
        self.assertEqual(rc, 1)
        self.assertIn("smbios_gui binary: expected 1 -> actual 0", lines[0])

    def test_CliFindBinary(self) -> None:
        self.put("smbios_gui/bin/smbios_gui")
        proc = subprocess.run([sys.executable, "-I", str(SCRIPT), "find-binary", "--build-dir",
                               "build/linux-gui"], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              env=neutral_env(self.tmp), cwd=str(self.tmp), timeout=60, text=True)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertTrue(proc.stdout.startswith("ok: smbios_gui: build/linux-gui/smbios_gui/bin/"))


class TotalTestsTests(unittest.TestCase):
    """``Total Tests:`` parsing and ``ctest_total`` (fail closed: ``None`` is never 0)."""

    def test_ParseTotals(self) -> None:
        cases = {
            b"Test project /x\n\nTotal Tests: 0\n": (0,),
            b"Total Tests: 0\r\n": (0,),
            b"  Total Tests:   7  \n": (7,),
            b"Total Tests: 2\nTotal Tests: 0\n": (2, 0),
            b"": (),
            b"Total Tests: x\n": (),
            b"Total Tests: -1\n": (),
            b"Total Tests: 0 extra\n": (),
            b"Total Tests:\n": (),
            b"No tests were found!!!\n": (),
            "Total Tests: \u0660\n".encode("utf-8"): (),  # Arabic-Indic zero is not [0-9]
        }
        for output, want in cases.items():
            with self.subTest(output=output):
                self.assertEqual(gg.parse_totals(output), want)

    def test_CtestTotal(self) -> None:
        def run(out: str, rc: int = 0) -> Optional[int]:
            return gg.ctest_total(_py(f"import sys; sys.stdout.write({out!r}); sys.exit({rc})"))
        self.assertEqual(run("Total Tests: 0\n"), 0)
        self.assertEqual(run("Total Tests: 0\r\n"), 0)
        self.assertEqual(run("Total Tests: 2\n"), 2)
        self.assertIsNone(run(""), "missing line is a failure, not 0")
        self.assertIsNone(run("Total Tests: 0\n", 1), "ctest exit 1 plus Total Tests: 0")
        self.assertIsNone(run("Total Tests: 0\nTotal Tests: 0\n"), "two lines")
        self.assertIsNone(run("Total Tests: zero\n"), "non-number")
        self.assertIsNone(gg.ctest_total([str(Path(tempfile.gettempdir()) / "no-such-ctest")]))

    def test_StderrIsMergedLikeTheOld2To1(self) -> None:
        listing = gg.ctest_listing(_py("import sys; sys.stderr.write('Total Tests: 0\\n')"))
        self.assertEqual((listing.rc, listing.totals), (0, (0,)))

    def test_TimeoutFailsClosed(self) -> None:
        listing = gg.ctest_listing(_py("import time; print('Total Tests: 0'); time.sleep(30)"),
                                   timeout_s=1.0)
        self.assertEqual(listing.rc, 124)
        self.assertIsNotNone(gg._listing_problem("x", listing), "a time-out is never a 0")


class LocalTierArtifactsTests(unittest.TestCase):
    """R-8 walk: files, directories and symlinks; ``casefold()``; errors fail closed."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.build = Path(self._tmp.name) / "build"
        (self.build / "CMakeFiles" / "smbios_gui.dir").mkdir(parents=True)
        (self.build / "smbios_gui" / "bin").mkdir(parents=True)
        (self.build / "smbios_gui" / "bin" / "smbios_gui").write_bytes(FAKE_BINARY)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def test_CleanTreeIsEmpty(self) -> None:
        self.assertEqual(gg.local_tier_artifacts(self.build), [])

    def test_DirectoryOnlyIsFound(self) -> None:
        d = self.build / "CMakeFiles" / "smbios_gui_autoexit.dir"
        d.mkdir()
        self.assertEqual(gg.local_tier_artifacts(self.build), [d])

    def test_FilesAndCaseVariantsAreFound(self) -> None:
        for name in ("smbios_gui_local_test", "smbios_gui_local_test.exe",
                     "SMBIOS_GUI_LOCAL_TEST.exe", "Smbios_Gui_AutoExit.pdb"):
            with self.subTest(name=name):
                path = self.build / "tests" / name
                path.parent.mkdir(exist_ok=True)
                path.write_bytes(b"")
                self.assertEqual(gg.local_tier_artifacts(self.build), [path])
                path.unlink()

    @unittest.skipIf(os.name == "nt", "symlinks need privileges on Windows")
    def test_SymlinksAreFoundNotFollowed(self) -> None:
        dangling = self.build / "smbios_gui_autoexit"
        dangling.symlink_to(self.build / "nowhere")
        self.assertEqual(gg.local_tier_artifacts(self.build), [dangling])
        dangling.unlink()
        outside = Path(self._tmp.name) / "outside"
        (outside / "smbios_gui_local_test").mkdir(parents=True)
        dirlink = self.build / "smbios_gui_autoexit_link"
        dirlink.symlink_to(outside, target_is_directory=True)
        self.assertEqual(gg.local_tier_artifacts(self.build), [dirlink],
                         "the link itself counts; its target is not walked")

    def test_MissingTreeRaises(self) -> None:
        with self.assertRaises(OSError):
            gg.local_tier_artifacts(self.build / "missing")
        (self.build / "file").write_bytes(b"")
        with self.assertRaises(OSError):
            gg.local_tier_artifacts(self.build / "file")

    @unittest.skipIf(os.name == "nt" or IS_ROOT, "needs POSIX permissions and a non-root user")
    def test_UnlistableDirectoryRaises(self) -> None:
        locked = self.build / "locked"
        locked.mkdir()
        locked.chmod(0)
        try:
            with self.assertRaises(PermissionError):
                gg.local_tier_artifacts(self.build)
        finally:
            locked.chmod(stat.S_IRWXU)


@unittest.skipIf(os.name == "nt", POSIX_ONLY)
class LocalTierAbsentTests(unittest.TestCase):
    """R-8 / C20 end to end against a stub ``ctest`` (in-process and CLI)."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        self.build = self.tmp / "build" / "linux-gui"
        (self.build / "CMakeFiles" / "smbios_gui.dir").mkdir(parents=True)
        (self.build / gg.CTEST_FILE).write_text("subdirs(\"tests\")\n", encoding="ascii")
        self.bin = self.tmp / "bin"
        self._path = mock.patch.dict(os.environ, {"PATH": path_with(self.bin)})
        self._path.start()
        self.run_check = _Captured(self.tmp)

    def tearDown(self) -> None:
        self._path.stop()
        self._tmp.cleanup()

    def ctest(self, label_out: str, name_out: Optional[str] = None, label_rc: int = 0,
              name_rc: int = 0, ci_out: str = "Test project x\n\nTotal Tests: 4\n",
              ci_rc: int = 0) -> None:
        """Stub ctest: ``-L gui-ci`` -> ``ci_out``, other ``-L`` -> ``label_out``, else ``name_out``."""
        name_out = label_out if name_out is None else name_out
        write_stub(self.bin, "ctest",
                   "import sys\n"
                   "if 'gui-ci' in sys.argv:\n"
                   f"    sys.stdout.write({ci_out!r})\n"
                   f"    sys.exit({ci_rc})\n"
                   f"by_label = '-L' in sys.argv\n"
                   f"sys.stdout.write({label_out!r} if by_label else {name_out!r})\n"
                   f"sys.exit({label_rc} if by_label else {name_rc})\n")

    def check(self, config: Optional[str] = "Debug"):
        return self.run_check(gg.check_local_tier_absent, self.build, config)

    def test_Passes(self) -> None:
        self.ctest("Test project x\n\nTotal Tests: 0\n")
        rc, lines = self.check()
        self.assertEqual(rc, 0, lines)
        self.assertEqual(len(lines), 1)
        self.assertRegex(lines[0], r"^ok: no gui-local tests configured \(\d+\.\ds\)$")

    def test_CrlfPasses(self) -> None:
        self.ctest("Test project x\r\n\r\nTotal Tests: 0\r\n")
        self.assertEqual(self.check()[0], 0)

    def test_ConfiguredTestsFail(self) -> None:
        self.ctest("  Test #1: smbios_gui_local_test_x\n\nTotal Tests: 2\n",
                   name_out="Total Tests: 0\n")
        rc, lines = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("ctest -N -L gui-local: local-only GUI tests are configured on CI: "
                      "expected 0 -> 2", lines[0])

    def test_ByNameCatchesAMissingLabel(self) -> None:
        self.ctest("Total Tests: 0\n", name_out="Total Tests: 1\n")
        rc, lines = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("smbios_gui_local_test|smbios_gui_autoexit: local-only GUI tests are "
                      "configured on CI: expected 0 -> 1", lines[0])

    def test_FailClosedPaths(self) -> None:
        cases = {
            "missing line": (dict(label_out="Test project x\n"),
                             "'Total Tests:' line: expected present -> missing (exit 0)"),
            "missing line, ctest failed": (dict(label_out="", label_rc=8),
                                           "'Total Tests:' line: expected present -> missing "
                                           "(exit 8)"),
            "exit 1 with Total Tests: 0": (dict(label_out="Total Tests: 0\n", label_rc=1),
                                           "ctest exit: expected 0 -> 1"),
            "two lines": (dict(label_out="Total Tests: 0\nTotal Tests: 0\n"),
                          "'Total Tests:' lines: expected 1 -> 2"),
            "non-number": (dict(label_out="Total Tests: none\n"),
                           "'Total Tests:' line: expected present -> missing (exit 0)"),
        }
        for name, (kwargs, want) in cases.items():
            with self.subTest(case=name):
                self.ctest(name_out="Total Tests: 0\n", **kwargs)
                rc, lines = self.check()
                self.assertEqual(rc, 1, lines)
                self.assertIn(want, lines[0])

    def test_CtestMissingFailsClosed(self) -> None:
        with mock.patch.dict(os.environ, {"PATH": str(self.tmp / "empty")}):
            rc, lines = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("ctest could not run (ENOENT)", lines[0])

    def test_DirectoryOnlyBuildTreeFails(self) -> None:
        self.ctest("Total Tests: 0\n")
        (self.build / "CMakeFiles" / "smbios_gui_autoexit.dir").mkdir()
        rc, lines = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("local-only GUI files exist on CI: expected 0 -> 1", lines[0])
        self.assertIn("  CMakeFiles/smbios_gui_autoexit.dir", lines)

    def test_AtMostFivePathsAndHostileNamesHidden(self) -> None:
        self.ctest("Total Tests: 0\n")
        for i in range(7):
            (self.build / f"smbios_gui_local_test{i}").write_bytes(b"")
        (self.build / "smbios_gui_autoexit ::error::x").mkdir()
        rc, lines = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("expected 0 -> 8", lines[0])
        shown = [x for x in lines if x.startswith("  ")]
        self.assertEqual(len(shown), gg.MAX_SHOWN_PATHS)
        self.assertNotIn("::error::x", "\n".join(lines))
        self.assertIn("  <path>", shown)

    def test_MissingBuildTreeFailsClosed(self) -> None:
        self.ctest("Total Tests: 0\n")
        self.build = self.tmp / "nope"
        rc, lines = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("local-only GUI file check: walk failed (ENOENT)", "\n".join(lines))

    def test_AllProblemsAreReported(self) -> None:
        self.ctest("", name_out="Total Tests: 3\n")
        (self.build / "smbios_gui_autoexit").write_bytes(b"")
        rc, lines = self.check()
        self.assertEqual(rc, 1)
        text = "\n".join(lines)
        self.assertIn("-L gui-local: 'Total Tests:' line: expected present -> missing", text)
        self.assertIn("expected 0 -> 3", text)
        self.assertIn("local-only GUI files exist on CI: expected 0 -> 1", text)

    def test_ConfigIsPassedAndValidated(self) -> None:
        argv_file = self.tmp / "argv"
        write_stub(self.bin, "ctest", "import sys\n"
                   f"open({str(argv_file)!r}, 'a').write(' '.join(sys.argv[1:]) + '\\n')\n"
                   "print('Total Tests: 2' if 'gui-ci' in sys.argv else 'Total Tests: 0')\n")
        self.assertEqual(self.check("Debug")[0], 0)
        self.assertEqual(self.check(None)[0], 0)
        lines = argv_file.read_text().splitlines()
        self.assertEqual(lines[0], f"--test-dir {self.build} -C Debug -N -L gui-local")
        self.assertEqual(lines[1], f"--test-dir {self.build} -C Debug -N -R "
                                   "smbios_gui_local_test|smbios_gui_autoexit")
        self.assertEqual(lines[2], f"--test-dir {self.build} -C Debug -N -L gui-ci")
        self.assertEqual(lines[3], f"--test-dir {self.build} -N -L gui-local")
        self.assertEqual(lines[5], f"--test-dir {self.build} -N -L gui-ci")
        self.assertEqual(self.check("Debug;rm")[0], 1)

    # --- S3-3: a wrong or unconfigured --build-dir never passes -------------

    def test_UnconfiguredTreeFailsLikeRealCtest(self) -> None:
        """S3-3 regression: real ctest prints ``Total Tests: 0``/exit 0 for any directory."""
        self.ctest("Total Tests: 0\n", ci_out="Total Tests: 0\n")
        (self.build / gg.CTEST_FILE).unlink()
        rc, lines = self.check()
        self.assertEqual(rc, 1, lines)
        text = "\n".join(lines)
        self.assertIn("configured build tree: CTestTestfile.cmake: expected regular file -> "
                      "ENOENT", text)
        self.assertIn("ctest -N -L gui-ci: configured gui-ci tests: expected > 0 -> 0", text)
        self.assertFalse(any(x.startswith("ok:") for x in lines))

    def test_MissingCtestFileAloneFails(self) -> None:
        self.ctest("Total Tests: 0\n")
        (self.build / gg.CTEST_FILE).unlink()
        rc, lines = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("CTestTestfile.cmake: expected regular file -> ENOENT", lines[0])

    def test_NonRegularCtestFileFails(self) -> None:
        self.ctest("Total Tests: 0\n")
        (self.build / gg.CTEST_FILE).unlink()
        (self.build / gg.CTEST_FILE).mkdir()
        rc, lines = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("CTestTestfile.cmake: expected regular file -> not a regular file",
                      lines[0])
        (self.build / gg.CTEST_FILE).rmdir()
        (self.build / gg.CTEST_FILE).symlink_to(self.tmp / "elsewhere.cmake")
        (self.tmp / "elsewhere.cmake").write_text("", encoding="ascii")
        rc, lines = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("CTestTestfile.cmake: expected regular file -> not a regular file",
                      lines[0])

    def test_NoGuiCiTestsFails(self) -> None:
        self.ctest("Total Tests: 0\n", ci_out="Total Tests: 0\n")
        rc, lines = self.check()
        self.assertEqual(rc, 1)
        self.assertIn("ctest -N -L gui-ci: configured gui-ci tests: expected > 0 -> 0", lines[0])

    def test_GuiCiListingFailClosedPaths(self) -> None:
        cases = {"missing line": (dict(ci_out="Test project x\n"),
                                  "'Total Tests:' lines: expected 1 -> 0 (exit 0)"),
                 "two lines": (dict(ci_out="Total Tests: 3\nTotal Tests: 3\n"),
                               "'Total Tests:' lines: expected 1 -> 2"),
                 "exit 1": (dict(ci_out="Total Tests: 3\n", ci_rc=1),
                            "ctest exit: expected 0 -> 1")}
        for name, (kwargs, want) in cases.items():
            with self.subTest(case=name):
                self.ctest("Total Tests: 0\n", **kwargs)
                rc, lines = self.check()
                self.assertEqual(rc, 1, lines)
                self.assertIn(f"ctest -N -L gui-ci: {want}", lines[0])

    def test_ConfiguredTreeWithGuiCiTestsPasses(self) -> None:
        self.ctest("Total Tests: 0\n", ci_out="Total Tests: 1\n")
        self.assertEqual(self.check()[0], 0)

    def test_Cli(self) -> None:
        self.ctest("Total Tests: 0\n")
        env = neutral_env(self.tmp, PATH=path_with(self.bin))
        argv = [sys.executable, "-I", str(SCRIPT), "local-tier-absent", "--build-dir",
                str(self.build), "--config", "Debug"]
        proc = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env,
                              timeout=60, text=True)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertTrue(proc.stdout.startswith("ok: no gui-local tests configured"))
        (self.build / "smbios_gui_local_test.exe").write_bytes(b"")
        proc = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env,
                              timeout=60, text=True)
        self.assertEqual(proc.returncode, 1)


if __name__ == "__main__":
    unittest.main()
