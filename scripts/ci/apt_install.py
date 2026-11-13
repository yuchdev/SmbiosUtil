#!/usr/bin/env python3
"""Install the GUI leg's apt packages with a DNS-checked mirror fallback (milestone 0014, Story 02.0 Task 02).

Replaces milestone 0011's inline ``gui-build`` apt step (inventory C15) and
keeps all of its behaviour:

1. Packages come from ``scripts/ci/apt-packages-gui.txt`` (the 0011 list).
2. Every host named in the active apt sources (``sources.list.d/*.sources``,
   ``sources.list.d/*.list``, ``sources.list`` and ``apt-mirrors.txt``) is
   resolved with ``getent hosts`` (5 s each). If any host does not resolve,
   ``apt-mirrors.txt`` is replaced by the kernel.org Ubuntu mirror (the 0011
   mechanism). One line reports the choice::

       info: apt mirror: <default|kernel.org> (<k> of <n> hosts unresolved: <names>)

   A host name is printed only if it matches ``^[A-Za-z0-9.-]{1,253}$``, else
   as ``<invalid>`` (threat model CR-4); every line goes through
   ``Reporter.info``.
3. ``apt-get update`` and ``apt-get install`` run through ``ci_step`` (argv,
   no shell) with the 0011 options (``DEBIAN_FRONTEND=noninteractive``,
   ``DPkg::Lock::Timeout=120``, ``Acquire::Retries=1``,
   ``Acquire::http(s)::Timeout=20``, ``-qq --no-install-recommends``) and a
   runner-side time limit each, so a stall fails in minutes with RC1 output.
4. An ``apt-get update`` that exits 0 but leaves no candidate for the first
   package (``apt-cache policy``) is a hard failure (plan E3, the silent
   "succeeded with nothing fetched" case), with an excerpt of the update log.
5. One ``info: apt security pocket: ...`` line names where the security
   pocket comes from: the fallback deliberately does not handle it (0011
   follow-up), and the line makes that visible.

``sudo`` is used only for apt and for installing the mirror file (``sudo
install -m 0644``: ``ci_step`` closes the child's stdin, so 0011's ``sudo tee``
cannot be used), never to run this script.

Usage::

    python3 scripts/ci/apt_install.py [--packages FILE] [--reporter NAME]

Exit codes: 0 ok, the failing apt command's code (124 on a time-out), 1 for
the no-candidate case, 2 usage or environment error. Python 3 standard
library only.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import argparse  # noqa: E402
import re  # noqa: E402
import subprocess  # noqa: E402
import tempfile  # noqa: E402
import time  # noqa: E402
from dataclasses import dataclass  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import Final, List, Optional, Sequence, Tuple  # noqa: E402

import ci_step  # noqa: E402
import env_names  # noqa: E402
from reporters import Reporter, child_environment, get_reporter  # noqa: E402

#: apt configuration root (overridable for tests with ``--etc-apt``).
ETC_APT: Final = Path("/etc/apt")
#: Default package list.
PACKAGES_FILE: Final = Path(_HERE) / "apt-packages-gui.txt"
#: The 0011 fallback: ``apt-mirrors.txt`` content (verified on ubuntu-24.04).
FALLBACK_MIRROR_LINE: Final = "https://mirrors.edge.kernel.org/ubuntu/\tpriority:1\n"
#: The 0011 apt invocation, without the subcommand.
APT_GET: Final = ("sudo", "DEBIAN_FRONTEND=noninteractive", "apt-get",
                  "-o", "DPkg::Lock::Timeout=120", "-o", "Acquire::Retries=1",
                  "-o", "Acquire::http::Timeout=20", "-o", "Acquire::https::Timeout=20")
#: Runner-side limits; together they stay inside the 10-minute step budget.
DNS_TIMEOUT_S: Final = 5.0
UPDATE_TIMEOUT_S: Final = 180.0
POLICY_TIMEOUT_S: Final = 30.0
INSTALL_TIMEOUT_S: Final = 360.0
#: Host names printed in the mirror line before ``+<n> more``.
MAX_NAMES: Final = 5

#: CR-4 allowlist for a printed host name.
HOST_OK = re.compile(r"^[A-Za-z0-9.-]{1,253}$")
_PACKAGE_OK = re.compile(r"^[a-z0-9][a-z0-9+.-]{0,99}$")
_ONE_LINE = re.compile(r"^\s*deb(?:-src)?\s+(?:\[[^\]]*\]\s+)?(\S+)\s+(\S+)")
_CANDIDATE = re.compile(r"^\s*Candidate:\s*(\S+)", re.MULTILINE)


@dataclass(frozen=True)
class AptSource:
    """One source URI and the suites it serves (empty for ``apt-mirrors.txt``)."""

    uri: str
    suites: Tuple[str, ...]


def host_of(uri: str) -> Optional[str]:
    """Return the host part of an apt URI, or ``None`` for local/mirror-file URIs.

    The host is returned unvalidated (callers apply :data:`HOST_OK`).
    """
    scheme, sep, rest = uri.partition("://")
    if not sep or scheme.lower() in ("file", "cdrom", "copy", "mirror+file"):
        return None
    authority = rest.split("/", 1)[0].rpartition("@")[2]
    authority = re.sub(r":\d+$", "", authority)
    return authority or None


def _deb822(text: str) -> List[AptSource]:
    out: List[AptSource] = []
    for stanza in re.split(r"\n\s*\n", text):
        uris: List[str] = []
        suites: List[str] = []
        for line in stanza.splitlines():
            if line.lstrip().startswith("#") or line[:1].isspace():
                continue
            key, sep, value = line.partition(":")
            if not sep:
                continue
            if key.strip().lower() == "uris":
                uris = value.split()
            elif key.strip().lower() == "suites":
                suites = value.split()
        out.extend(AptSource(u, tuple(suites)) for u in uris)
    return out


def _one_line(text: str) -> List[AptSource]:
    out: List[AptSource] = []
    for line in text.splitlines():
        match = _ONE_LINE.match(line)
        if match:
            out.append(AptSource(match.group(1), (match.group(2),)))
    return out


def _mirror_list(text: str) -> List[AptSource]:
    out: List[AptSource] = []
    for line in text.splitlines():
        fields = line.split()
        if fields and not fields[0].startswith("#"):
            out.append(AptSource(fields[0], ()))
    return out


def _read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def apt_sources(etc_apt: Path = ETC_APT) -> List[AptSource]:
    """Return every source entry from the active apt configuration under ``etc_apt``."""
    out: List[AptSource] = []
    parts = etc_apt / "sources.list.d"
    if parts.is_dir():
        for path in sorted(parts.glob("*.sources")):
            out.extend(_deb822(_read(path)))
        for path in sorted(parts.glob("*.list")):
            out.extend(_one_line(_read(path)))
    out.extend(_one_line(_read(etc_apt / "sources.list")))
    out.extend(_mirror_list(_read(etc_apt / "apt-mirrors.txt")))
    return out


def apt_source_hosts(etc_apt: Path = ETC_APT) -> List[str]:
    """Return the distinct hosts of every active apt source, in first-seen order."""
    hosts: List[str] = []
    for source in apt_sources(etc_apt):
        host = host_of(source.uri)
        if host and host not in hosts:
            hosts.append(host)
    return hosts


def unresolved(hosts: Sequence[str], timeout_s: float = DNS_TIMEOUT_S) -> List[str]:
    """Return the hosts that ``getent hosts`` cannot resolve within ``timeout_s`` each.

    A name failing :data:`HOST_OK` is never passed to a command and counts as
    unresolved.
    """
    bad: List[str] = []
    for host in hosts:
        if not HOST_OK.match(host):
            bad.append(host)
            continue
        try:
            rc = subprocess.run(["getent", "hosts", "--", host], shell=False,
                                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL, env=child_environment(os.environ),
                                timeout=timeout_s).returncode
        except (OSError, subprocess.TimeoutExpired):
            rc = 1
        if rc != 0:
            bad.append(host)
    return bad


def shown_hosts(hosts: Sequence[str]) -> str:
    """Return the CR-4-safe host list for an ``info:`` line (``none`` when empty)."""
    names = [h if HOST_OK.match(h) else "<invalid>" for h in hosts]
    if not names:
        return "none"
    more = f" +{len(names) - MAX_NAMES} more" if len(names) > MAX_NAMES else ""
    return ", ".join(names[:MAX_NAMES]) + more


@dataclass
class _Ctx:
    reporter: Reporter
    log_dir: Path
    results: Path


def _default_ctx(reporter: Optional[Reporter] = None) -> _Ctx:
    return _Ctx(reporter or get_reporter(None),
                Path(env_names.get("CI_LOG_DIR", required=True)),
                Path(env_names.get("CI_RESULTS_FILE", required=True)))


def use_fallback_mirror(etc_apt: Path = ETC_APT, *, ctx: Optional[_Ctx] = None) -> bool:
    """Point ``apt-mirrors.txt`` at the kernel.org Ubuntu mirror (the 0011 entry).

    The content is staged in ``$CI_TEMP_DIR`` and moved into place with
    ``sudo install -m 0644`` through ``ci_step``. Returns whether it worked
    (a failure is already reported).
    """
    ctx = ctx or _default_ctx()
    temp_dir = Path(env_names.get("CI_TEMP_DIR", required=True))
    temp_dir.mkdir(parents=True, exist_ok=True)
    fd, staged = tempfile.mkstemp(prefix="apt-mirrors-", suffix=".txt", dir=str(temp_dir))
    try:
        os.write(fd, FALLBACK_MIRROR_LINE.encode("ascii"))
    finally:
        os.close(fd)
    try:
        result = ci_step.run_step(
            "apt mirror fallback",
            ["sudo", "install", "-m", "0644", staged, str(etc_apt / "apt-mirrors.txt")],
            patterns=["apt"], best_effort_reason=None, summary="kernel.org",
            log_dir=ctx.log_dir, reporter=ctx.reporter, results_file=ctx.results,
            timeout_s=60.0)
    finally:
        os.unlink(staged)
    return result.exit == 0


def security_origin(sources: Sequence[AptSource]) -> str:
    """Return the ``info:`` text naming where the ``*-security`` pocket is fetched from."""
    direct: List[str] = []
    via_list = False
    for source in sources:
        if not any(s.endswith("-security") for s in source.suites):
            continue
        if source.uri.lower().startswith("mirror+"):
            via_list = True
            continue
        host = host_of(source.uri)
        if host and host not in direct:
            direct.append(host)
    if not direct and not via_list:
        return "apt security pocket: none configured"
    parts = []
    if direct:
        parts.append(f"{shown_hosts(direct)} (direct; the mirror fallback does not cover it)")
    if via_list:
        parts.append("apt-mirrors.txt (follows the mirror choice)")
    return "apt security pocket: " + "; ".join(parts)


def read_packages(path: Path) -> List[str]:
    """Return the package names in ``path``.

    Raises:
        ValueError: The file is unreadable, empty, or names an invalid package.
    """
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise ValueError(f"cannot read {path.name} ({type(exc).__name__})") from None
    names = [tok for line in text.splitlines() for tok in line.split("#", 1)[0].split()]
    if not names:
        raise ValueError(f"{path.name} lists no packages")
    bad = [n for n in names if not _PACKAGE_OK.match(n)]
    if bad:
        raise ValueError(f"{path.name}: invalid package name")
    return names


def _info(ctx: _Ctx, line: str) -> None:
    ci_step._emit(ctx.reporter.info(line))


def _run(ctx: _Ctx, label: str, argv: Sequence[str], timeout_s: float) -> ci_step.StepResult:
    return ci_step.run_step(label, argv, patterns=["apt"], best_effort_reason=None,
                            summary=None, log_dir=ctx.log_dir, reporter=ctx.reporter,
                            results_file=ctx.results, timeout_s=timeout_s)


def _no_candidate(ctx: _Ctx, package: str, elapsed: float) -> int:
    """Report the silent empty-update case: one annotation plus the update log excerpt."""
    label = "apt package lists"
    first = f"apt-get update exited 0 but {package} has no candidate (nothing fetched?)"
    update_log = ci_step.log_path_for("apt-get update", ctx.log_dir)
    excerpt = ci_step.render_failure("apt-get update", update_log, ["apt"],
                                     ci_step.MAX_DIAG, ci_step.MAX_TAIL)
    ci_step._emit(ci_step._render_bounded(
        lambda ex: ctx.reporter.step_fail(label, 1, elapsed, first, ex), excerpt))
    ci_step.append_record(ci_step.StepResult(label, "hard", "fail", 1, elapsed, "", first, None),
                          ctx.results)
    return 1


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="apt_install.py",
                                     description="Install apt packages with a mirror fallback.")
    parser.add_argument("--packages", default=str(PACKAGES_FILE))
    parser.add_argument("--etc-apt", default=str(ETC_APT), help=argparse.SUPPRESS)
    parser.add_argument("--dns-timeout", type=float, default=DNS_TIMEOUT_S)
    parser.add_argument("--update-timeout", type=float, default=UPDATE_TIMEOUT_S)
    parser.add_argument("--install-timeout", type=float, default=INSTALL_TIMEOUT_S)
    parser.add_argument("--reporter", default=None)
    return parser


def main(argv: Sequence[str]) -> int:
    """Run the C15 install; return the exit code."""
    args = _parser().parse_args(list(argv))
    try:
        reporter = get_reporter(args.reporter)
        packages = read_packages(Path(args.packages))
    except ValueError as exc:
        ci_step.emit_error(f"apt_install.py: error: {exc}")
        return env_names.EXIT_ENV_ERROR
    ctx = _default_ctx(reporter)
    etc_apt = Path(args.etc_apt)

    sources = apt_sources(etc_apt)
    hosts = apt_source_hosts(etc_apt)
    bad = unresolved(hosts, args.dns_timeout)
    mirror = "default"
    if bad:
        if not use_fallback_mirror(etc_apt, ctx=ctx):
            return 1
        mirror = "kernel.org"
    _info(ctx, f"info: apt mirror: {mirror} ({len(bad)} of {len(hosts)} hosts unresolved: "
               f"{shown_hosts(bad)})")
    _info(ctx, f"info: {security_origin(sources)}")

    start = time.monotonic()
    update = _run(ctx, "apt-get update", [*APT_GET, "update", "-qq"], args.update_timeout)
    if update.exit != 0:
        return update.exit
    policy = _run(ctx, "apt-cache policy", ["apt-cache", "policy", packages[0]], POLICY_TIMEOUT_S)
    if policy.exit != 0:
        return policy.exit
    log = ci_step.log_path_for("apt-cache policy", ctx.log_dir)
    try:
        match = _CANDIDATE.search(log.read_text(encoding="utf-8", errors="replace")[:65536])
    except OSError:
        match = None
    if match is None or match.group(1) == "(none)":
        return _no_candidate(ctx, packages[0], round(time.monotonic() - start, 1))
    install = _run(ctx, "apt-get install",
                   [*APT_GET, "install", "-y", "-qq", "--no-install-recommends", *packages],
                   args.install_timeout)
    return install.exit


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step._emit([f"error: apt_install.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
