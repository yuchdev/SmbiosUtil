"""Tests for the apt install with mirror fallback (milestone 0014, Story 02.0 Task 02).

Covers ``scripts/ci/apt_install.py``: source-host discovery (deb822, one-line
and ``apt-mirrors.txt``), the DNS check of every host, the kernel.org fallback,
the 0011 apt options, the silent "update fetched nothing" failure, the
time-limited install, the security-pocket line and the CR-4 host allowlist.

Classification: unit (parsing) and mock (end-to-end with stub ``sudo``,
``getent``, ``apt-get`` and ``apt-cache`` on a temporary ``PATH``; a temporary
``etc/apt`` tree). No network, no real apt, no root. POSIX only for the stub
runs (the script is Linux-only; stubs are ``/bin/sh`` trampolines).
On failure first check: the stub's environment switches (``STUB_*``) and the
fixture ``etc/apt`` written in ``setUp``; then ``apt_install.host_of``.
Real-runner behaviour (actual mirror DNS, the ``info: apt mirror:`` line, the
GUI leg building) is verified in Story 03.0 Task 03, not here.

Run: ``python3 scripts/ci/run_tests.py``.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from typing import Dict, List, Tuple

from ci_test_support import CI_DIR, neutral_env, write_stub

import apt_install  # noqa: E402  (ci_test_support put scripts/ci on sys.path)

SCRIPT = CI_DIR / "apt_install.py"
POSIX_ONLY = "stub commands are /bin/sh trampolines; apt_install.py is Linux-only"
#: Hostile host names (CR-4), built by concatenation like the sibling tests.
HOSTILE = ("::" + "error::x", "##" + "teamcity[x]")

UBUNTU_SOURCES = """\
Types: deb
URIs: mirror+file:/etc/apt/apt-mirrors.txt
Suites: noble noble-updates noble-backports
Components: main universe

# security pocket, fetched directly
Types: deb
URIs: http://security.ubuntu.com/ubuntu
Suites: noble-security
Components: main universe
"""
MIRRORS = "http://azure.archive.ubuntu.com/ubuntu/\tpriority:1\nhttps://archive.ubuntu.com/ubuntu/\tpriority:2\n"
MS_LIST = "deb [arch=amd64,arm64 signed-by=/k.gpg] https://packages.microsoft.com/ubuntu/24.04/prod noble main\n"

SUDO_STUB = """\
import os, sys
args = sys.argv[1:]
env = dict(os.environ)
while args and '=' in args[0] and not args[0].startswith('-'):
    k, v = args.pop(0).split('=', 1)
    env[k] = v
os.execvpe(args[0], args, env)
"""
GETENT_STUB = """\
import os, sys
bad = [h for h in os.environ.get('STUB_UNRESOLVED', '').split(',') if h]
host = sys.argv[-1]
if sys.argv[1:3] != ['hosts', '--'] or host in bad:
    sys.exit(2)
print('10.0.0.1 ' + host)
"""
APT_GET_STUB = """\
import os, sys, time
with open(os.environ['STUB_CALLS'], 'a') as fh:
    fh.write(os.environ.get('DEBIAN_FRONTEND', '-') + ' ' + ' '.join(sys.argv[1:]) + '\\n')
if 'update' in sys.argv:
    print('Reading package lists...', flush=True)
if 'install' in sys.argv and os.environ.get('STUB_APT_SLEEP'):
    print('Unpacking libx11-dev ...', flush=True)
    time.sleep(float(os.environ['STUB_APT_SLEEP']))
"""
APT_CACHE_STUB = """\
import os, sys
pkg = sys.argv[-1]
print(pkg + ':')
print('  Installed: (none)')
print('  Candidate: ' + ('(none)' if os.environ.get('STUB_NO_CANDIDATE') else '2:1.1.4-3build1'))
"""


class HostParsingTests(unittest.TestCase):
    """Unit: which hosts the DNS check covers."""

    def setUp(self) -> None:
        self._td = tempfile.TemporaryDirectory()
        self.addCleanup(self._td.cleanup)
        self.etc = Path(self._td.name)
        (self.etc / "sources.list.d").mkdir()
        (self.etc / "sources.list.d" / "ubuntu.sources").write_text(UBUNTU_SOURCES)
        (self.etc / "sources.list.d" / "microsoft-prod.list").write_text(MS_LIST)
        (self.etc / "apt-mirrors.txt").write_text(MIRRORS)
        (self.etc / "sources.list").write_text("# empty on 24.04\n")

    def test_EveryConfiguredHostIsProbed(self) -> None:
        self.assertEqual(apt_install.apt_source_hosts(self.etc),
                         ["security.ubuntu.com", "packages.microsoft.com",
                          "azure.archive.ubuntu.com", "archive.ubuntu.com"])

    def test_HostOfEdgeCases(self) -> None:
        self.assertIsNone(apt_install.host_of("mirror+file:/etc/apt/apt-mirrors.txt"))
        self.assertIsNone(apt_install.host_of("file:/srv/repo"))
        self.assertEqual(apt_install.host_of("http://u:p@h.example:8080/x"), "h.example")
        self.assertEqual(apt_install.host_of("mirror+https://m.example/ubuntu"), "m.example")

    def test_MissingConfigurationGivesNoHosts(self) -> None:
        self.assertEqual(apt_install.apt_source_hosts(self.etc / "absent"), [])

    def test_SecurityPocketOrigin(self) -> None:
        text = apt_install.security_origin(apt_install.apt_sources(self.etc))
        self.assertIn("security.ubuntu.com (direct", text)
        self.assertEqual(apt_install.security_origin([]), "apt security pocket: none configured")

    def test_InvalidNamesAreNeverProbed(self) -> None:
        self.assertEqual(apt_install.unresolved([HOSTILE[0], HOSTILE[1]], timeout_s=0.1), list(HOSTILE))

    def test_PackageListIs0011List(self) -> None:
        self.assertEqual(apt_install.read_packages(apt_install.PACKAGES_FILE),
                         ["libxinerama-dev", "libxcursor-dev", "xorg-dev", "libglu1-mesa-dev",
                          "pkg-config", "autoconf", "autoconf-archive", "automake", "libtool"])

    def test_InvalidPackageNameRejected(self) -> None:
        bad = self.etc / "pkgs.txt"
        bad.write_text("ok-pkg ;rm\n")
        with self.assertRaises(ValueError):
            apt_install.read_packages(bad)


@unittest.skipIf(os.name == "nt", POSIX_ONLY)
class AptInstallRunTests(unittest.TestCase):
    """Mock: the whole script against stub commands."""

    def setUp(self) -> None:
        self._td = tempfile.TemporaryDirectory(prefix="ci-apt-test-")
        self.addCleanup(self._td.cleanup)
        self.tmp = Path(self._td.name)
        self.etc = self.tmp / "etc" / "apt"
        (self.etc / "sources.list.d").mkdir(parents=True)
        (self.etc / "sources.list.d" / "ubuntu.sources").write_text(UBUNTU_SOURCES)
        (self.etc / "apt-mirrors.txt").write_text(MIRRORS)
        stubs = self.tmp / "bin"
        write_stub(stubs, "sudo", SUDO_STUB)
        write_stub(stubs, "getent", GETENT_STUB)
        write_stub(stubs, "apt-get", APT_GET_STUB)
        write_stub(stubs, "apt-cache", APT_CACHE_STUB)
        self.stubs = stubs
        self.calls = self.tmp / "calls.txt"

    def run_script(self, *extra: str, **stub_env: str) -> Tuple[int, List[str]]:
        env: Dict[str, str] = neutral_env(self.tmp)
        env["PATH"] = str(self.stubs) + os.pathsep + env.get("PATH", "")
        env["STUB_CALLS"] = str(self.calls)
        env.update(stub_env)
        proc = subprocess.run([sys.executable, "-I", str(SCRIPT), "--etc-apt", str(self.etc),
                               *extra], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              stdin=subprocess.DEVNULL, env=env, timeout=60)
        return proc.returncode, proc.stdout.decode("utf-8").splitlines()

    def apt_calls(self) -> List[str]:
        return self.calls.read_text().splitlines() if self.calls.exists() else []

    def test_AllHostsResolveKeepsDefaultMirror(self) -> None:
        rc, lines = self.run_script()
        self.assertEqual(rc, 0, lines)
        self.assertIn("info: apt mirror: default (0 of 3 hosts unresolved: none)", lines)
        self.assertEqual((self.etc / "apt-mirrors.txt").read_text(), MIRRORS)
        update, install = self.apt_calls()
        for call in (update, install):
            self.assertTrue(call.startswith("noninteractive -o DPkg::Lock::Timeout=120 "
                                            "-o Acquire::Retries=1 -o Acquire::http::Timeout=20 "
                                            "-o Acquire::https::Timeout=20 "), call)
        self.assertTrue(update.endswith(" update -qq"), update)
        self.assertTrue(install.endswith(" install -y -qq --no-install-recommends "
                                         + " ".join(apt_install.read_packages(
                                             apt_install.PACKAGES_FILE))), install)
        self.assertTrue(any(ln.startswith("info: apt security pocket: security.ubuntu.com")
                            for ln in lines), lines)
        self.assertTrue(any(ln.startswith("ok: apt-get install (") for ln in lines), lines)

    def test_OneUnresolvedHostSwitchesToKernelOrg(self) -> None:
        rc, lines = self.run_script(STUB_UNRESOLVED="azure.archive.ubuntu.com")
        self.assertEqual(rc, 0, lines)
        self.assertIn("info: apt mirror: kernel.org (1 of 3 hosts unresolved: "
                      "azure.archive.ubuntu.com)", lines)
        self.assertEqual((self.etc / "apt-mirrors.txt").read_text(),
                         apt_install.FALLBACK_MIRROR_LINE)

    def test_UpdateWithNoCandidateIsHardFailure(self) -> None:
        rc, lines = self.run_script(STUB_NO_CANDIDATE="1")
        self.assertEqual(rc, 1, lines)
        failure = [ln for ln in lines if ln.startswith("error: ")]
        self.assertEqual(len(failure), 1, lines)
        self.assertIn("apt package lists failed (exit 1", failure[0])
        self.assertIn("libxinerama-dev has no candidate", failure[0])
        self.assertIn("--- apt-get update: last 1 lines ---", lines)
        self.assertIn("Reading package lists...", lines)
        self.assertEqual(len(self.apt_calls()), 1, "install must not run")

    def test_StalledInstallIsKilledWithRc1Output(self) -> None:
        start = time.monotonic()
        rc, lines = self.run_script("--install-timeout", "1", STUB_APT_SLEEP="60")
        self.assertLess(time.monotonic() - start, 30.0)
        self.assertEqual(rc, 124, lines)
        failure = [ln for ln in lines if ln.startswith("error: apt-get install failed (exit 124")]
        self.assertEqual(len(failure), 1, lines)
        self.assertIn("timed out after 1s", failure[0])
        self.assertIn("Unpacking libx11-dev ...", lines)

    def test_HostileHostNamesPrintInvalid(self) -> None:
        (self.etc / "sources.list.d" / "evil.sources").write_text(
            f"Types: deb\nURIs: http://{HOSTILE[0]}/ubuntu\nSuites: noble\nComponents: main\n")
        (self.etc / "sources.list").write_text(f"deb http://{HOSTILE[1]}/ubuntu noble main\n")
        rc, lines = self.run_script()
        self.assertEqual(rc, 0, lines)
        mirror = [ln for ln in lines if ln.startswith("info: apt mirror: kernel.org (2 of 5 ")]
        self.assertEqual(len(mirror), 1, lines)
        self.assertIn("<invalid>, <invalid>", mirror[0])
        text = "\n".join(lines)
        for needle in HOSTILE:
            self.assertNotIn(needle, text)


if __name__ == "__main__":
    unittest.main()
