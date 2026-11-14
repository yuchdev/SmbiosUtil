"""Tests for ``scripts/ci/install_actionlint.py`` (milestone 0014, Story 02.0 Task 03; CR-7).

Classification: unit. No network: the download is replaced by a fake that
writes a locally built archive. Covers single-member extraction, refusal of
``../x``, absolute, link and device members (tar and zip), the SHA-256 check
and the hash-checked archive cache.

On failure first check: ``extract_binary``'s member checks and ``install``'s
hash comparison.
"""

from __future__ import annotations

import hashlib
import io
import stat
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path
from typing import List, Optional, Sequence, Tuple

import ci_test_support  # noqa: F401  (puts scripts/ci on sys.path)

import install_actionlint as inst  # noqa: E402

BINARY = b"#!/bin/sh\necho fake actionlint\n"
KEY = ("linux", "amd64")


def make_tar(path: Path, members: Sequence[Tuple[str, str, bytes]]) -> Path:
    """Write a .tar.gz with (name, kind, data) members; kind is file/dir/sym/lnk/chr."""
    with tarfile.open(path, "w:gz") as tf:
        for name, kind, data in members:
            info = tarfile.TarInfo(name)
            if kind == "file":
                info.size = len(data)
                tf.addfile(info, io.BytesIO(data))
                continue
            info.type = {"dir": tarfile.DIRTYPE, "sym": tarfile.SYMTYPE,
                         "lnk": tarfile.LNKTYPE, "chr": tarfile.CHRTYPE}[kind]
            if kind in ("sym", "lnk"):
                info.linkname = "actionlint"
            tf.addfile(info)
    return path


def make_zip(path: Path, members: Sequence[Tuple[str, int, bytes]]) -> Path:
    """Write a .zip with (name, unix mode, data) members."""
    with zipfile.ZipFile(path, "w") as zf:
        for name, mode, data in members:
            info = zipfile.ZipInfo(name)
            info.external_attr = mode << 16
            zf.writestr(info, data)
    return path


class _Tmp(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="ci-actionlint-test-")
        self.tmp = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)
        self.dest = self.tmp / "dest"
        self.dest.mkdir()


class Extraction(_Tmp):
    """Only the single binary member is extracted; unsafe archives are refused."""

    def test_OnlyTheBinaryIsExtracted(self) -> None:
        archive = make_tar(self.tmp / "a.tar.gz", [("LICENSE.txt", "file", b"x"),
                                                   ("docs", "dir", b""),
                                                   ("actionlint", "file", BINARY)])
        tool = inst.extract_binary(archive, self.dest)
        self.assertEqual(tool.read_bytes(), BINARY)
        self.assertEqual([p.name for p in self.dest.iterdir()], ["actionlint"])
        self.assertTrue(tool.stat().st_mode & stat.S_IXUSR)

    def test_ZipBinaryIsExtracted(self) -> None:
        archive = make_zip(self.tmp / "a.zip", [("README.md", 0o100644, b"x"),
                                                ("actionlint.exe", 0o100755, BINARY)])
        self.assertEqual(inst.extract_binary(archive, self.dest).name, "actionlint.exe")

    def test_UnsafeTarMembersAreRefused(self) -> None:
        for name, kind in (("../x", "file"), ("/abs", "file"), ("a/../../x", "file"),
                           ("link", "sym"), ("hard", "lnk"), ("dev", "chr")):
            with self.subTest(member=name, kind=kind):
                archive = make_tar(self.tmp / f"bad-{kind}.tar.gz",
                                   [("actionlint", "file", BINARY), (name, kind, b"x")])
                with self.assertRaises(inst.InstallError) as ctx:
                    inst.extract_binary(archive, self.dest)
                self.assertEqual(ctx.exception.exit_code, inst.EXIT_REFUSED)
                self.assertEqual(list(self.dest.iterdir()), [], "nothing may be extracted")

    def test_UnsafeZipMembersAreRefused(self) -> None:
        for name, mode in (("../x", 0o100644), ("/abs", 0o100644), ("C:/x", 0o100644),
                           ("link", 0o120777)):
            with self.subTest(member=name):
                archive = make_zip(self.tmp / "bad.zip", [("actionlint.exe", 0o100755, BINARY),
                                                          (name, mode, b"x")])
                with self.assertRaises(inst.InstallError):
                    inst.extract_binary(archive, self.dest)
                self.assertEqual(list(self.dest.iterdir()), [])

    def test_NestedBinaryIsNotAccepted(self) -> None:
        archive = make_tar(self.tmp / "nested.tar.gz", [("sub/actionlint", "file", BINARY)])
        with self.assertRaises(inst.InstallError):
            inst.extract_binary(archive, self.dest)


class Install(_Tmp):
    """Download, hash check and cache (fake download, no network)."""

    def fake_fetch(self, payload: Path, calls: List[str]):
        def fetch(url: str, dest: Path) -> None:
            calls.append(url)
            dest.write_bytes(payload.read_bytes())
        return fetch

    def assets_for(self, payload: Path) -> dict:
        digest = hashlib.sha256(payload.read_bytes()).hexdigest()
        return {KEY: ("actionlint_test_linux_amd64.tar.gz", digest)}

    def test_HashMismatchIsRefused(self) -> None:
        payload = make_tar(self.tmp / "p.tar.gz", [("actionlint", "file", BINARY)])
        calls: List[str] = []
        cache = self.tmp / "cache"
        with self.assertRaises(inst.InstallError) as ctx:
            inst.install(cache, self.tmp / "t", key=KEY, fetch=self.fake_fetch(payload, calls),
                         assets={KEY: ("x.tar.gz", "0" * 64)})
        self.assertEqual(ctx.exception.exit_code, inst.EXIT_REFUSED)
        self.assertIn("sha256: expected", str(ctx.exception))
        self.assertEqual(list(cache.iterdir()), [], "a mismatching download is not kept")
        self.assertFalse((self.tmp / "t").exists(), "nothing extracted")

    def test_VerifiedInstallAndCacheReuse(self) -> None:
        payload = make_tar(self.tmp / "p.tar.gz", [("actionlint", "file", BINARY)])
        calls: List[str] = []
        assets = self.assets_for(payload)
        cache, temp = self.tmp / "cache", self.tmp / "t"
        first = inst.install(cache, temp, key=KEY, fetch=self.fake_fetch(payload, calls),
                             assets=assets)
        second = inst.install(cache, temp, key=KEY, fetch=self.fake_fetch(payload, calls),
                              assets=assets)
        self.assertEqual(len(calls), 1, "the cached archive is reused after a hash check")
        self.assertNotEqual(first.parent, second.parent, "each install gets a fresh mkdtemp")
        self.assertEqual(second.read_bytes(), BINARY)

    def test_TamperedCacheIsDownloadedAgain(self) -> None:
        payload = make_tar(self.tmp / "p.tar.gz", [("actionlint", "file", BINARY)])
        assets = self.assets_for(payload)
        cache = self.tmp / "cache"
        cache.mkdir()
        (cache / assets[KEY][0]).write_bytes(b"tampered")
        calls: List[str] = []
        inst.install(cache, self.tmp / "t", key=KEY, fetch=self.fake_fetch(payload, calls),
                     assets=assets)
        self.assertEqual(len(calls), 1)

    def test_UnsupportedHostIsEnvironmentError(self) -> None:
        with self.assertRaises(inst.InstallError) as ctx:
            inst.install(self.tmp, self.tmp, key=("plan9", "mips"))
        self.assertEqual(ctx.exception.exit_code, inst.EXIT_ENV)

    def test_PinnedAssetsAreWellFormed(self) -> None:
        for (os_name, arch), (asset, digest) in inst.ASSETS.items():
            self.assertIn(f"_{inst.VERSION}_{os_name}_{arch}.", asset)
            self.assertRegex(digest, r"^[0-9a-f]{64}$")
        self.assertEqual(inst.host_key("Linux", "x86_64"), ("linux", "amd64"))
        self.assertEqual(inst.host_key("Darwin", "arm64"), ("darwin", "arm64"))
        self.assertEqual(inst.host_key("Windows", "AMD64"), ("windows", "amd64"))


if __name__ == "__main__":
    unittest.main()
