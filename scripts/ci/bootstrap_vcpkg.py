#!/usr/bin/env python3
"""Bootstrap vcpkg at the pinned baseline commit (milestone 0014, Story 02.0 Task 02).

Replaces the ``lukka/run-vcpkg`` action (ADR 0005 P13) with the CR-7 behaviour
of the CI-rework threat model (``docs/security/2026-11-13-ci-rework.md``):

1. ``<sha>`` must be the same 40-hex value in ``vcpkg.json``
   (``builtin-baseline``), ``vcpkg-configuration.json``
   (``default-registry.baseline``) and ``scripts/ci/pins.json``
   (``vcpkg.commit``); otherwise exit 1 with
   ``vcpkg baseline: expected <a> -> actual <b>``.
2. In a fresh directory under ``$CI_TEMP_DIR`` (never the image's
   ``VCPKG_INSTALLATION_ROOT``, never a ``PATH`` lookup of ``vcpkg``):
   ``git init``, ``git fetch --depth 1 <VCPKG_URL> <sha>``,
   ``git checkout --detach FETCH_HEAD``.
3. ``git rev-parse HEAD`` must equal ``<sha>``.
4. ``bootstrap-vcpkg.sh|.bat -disableMetrics``. The POSIX bootstrap checks
   the downloaded tool against the SHA-512 in that commit's
   ``scripts/vcpkg-tool-metadata.txt`` itself (``vcpkgCheckEqualFileHash``);
   the Windows one (``scripts/bootstrap.ps1``) does not, and the metadata
   file holds no Windows hash. So this script checks the tool binary with
   ``hashlib`` on every platform: against the metadata file on macOS/Linux,
   and against ``pins.json`` ``vcpkg.windows_tool_sha512`` on Windows (whose
   ``tool_release_tag`` must equal the metadata's ``VCPKG_TOOL_RELEASE_TAG``).
5. Exports ``VCPKG_ROOT``, ``VCPKG_DISABLE_METRICS=1`` and
   ``VCPKG_BINARY_SOURCES`` (always starting ``clear``, threat model CR-2) to
   later sub-steps through this process's own environment (``os.environ``),
   never through a platform env file (P11). A caller such as ``stage.py``
   imports this module and calls :func:`main` in-process for that reason.
6. Prints exactly one line: ``ok: vcpkg bootstrap: <sha12>, tool <version>
   (<t>s)``. Sub-steps run through ``ci_step.run_step`` (argv, no shell,
   ``child_environment``) and stay silent when they pass; a failing sub-step
   prints its RC1 failure excerpt instead.

Usage::

    python3 scripts/ci/bootstrap_vcpkg.py [--cache-dir DIR] [--packaging-leg]
            [--repo-root DIR] [--pins FILE] [--reporter github|teamcity|plain]

Reads ``CI_EVENT``, ``CI_REF`` and ``CI_TEMP_DIR`` (neutral names only).
Exit codes: 0 ok, 1 failure, 2 usage or environment error, 70 internal error.
Python 3 standard library only.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import argparse  # noqa: E402
import hashlib  # noqa: E402
import json  # noqa: E402
import platform  # noqa: E402
import re  # noqa: E402
import tempfile  # noqa: E402
import time  # noqa: E402
from dataclasses import dataclass  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import Dict, Final, List, Mapping, Optional, Sequence  # noqa: E402

import ci_step  # noqa: E402
import env_names  # noqa: E402
import vcpkg_cache_report  # noqa: E402
from reporters import Reporter, get_reporter  # noqa: E402

#: The only vcpkg source (a constant, never an input; CR-7).
VCPKG_URL: Final = "https://github.com/microsoft/vcpkg"
#: Pins data file (ADR 0005 D3).
PINS_FILE: Final = Path(_HERE) / "pins.json"
#: Label of the single ``ok:`` line and of a script-level failure.
LABEL: Final = "vcpkg bootstrap"
#: Default ``files`` binary-cache directory name under ``$CI_TEMP_DIR``.
CACHE_DIR_NAME: Final = vcpkg_cache_report.CACHE_DIR_NAME
#: Per-call time limits (the step budget is 5 min, Task 03 enforces it).
GIT_TIMEOUT_S: Final = 180.0
BOOTSTRAP_TIMEOUT_S: Final = 180.0
VERSION_TIMEOUT_S: Final = 30.0

_SHA40 = re.compile(r"^[0-9a-f]{40}$")
_SHA512 = re.compile(r"^[0-9a-f]{128}$")
#: CR-4 allowlist for any value printed outside a fence.
_PRINTABLE = re.compile(r"^[0-9A-Za-z._-]{1,64}$")
#: Same allowlist as the RC4 header's version tokens (Task 01).
_VERSION_TOKEN = re.compile(r"^[0-9A-Za-z.+_-]{1,40}$")


class BootstrapError(Exception):
    """A bootstrap failure.

    Attributes:
        message: One-line reason for the RC1 failure line; ``""`` when the
            failing sub-step already printed its own RC1 output.
    """

    def __init__(self, message: str = "") -> None:
        super().__init__(message or "sub-step failed")
        self.message = message


class _QuietOk:
    """Reporter proxy that prints nothing for a passing sub-step.

    The script's contract is exactly one ``ok:`` line; failures keep the full
    RC1 rendering of the wrapped backend.
    """

    def __init__(self, inner: Reporter) -> None:
        self._inner = inner
        self.name = inner.name

    def step_ok(self, label: str, summary: str, elapsed_s: float) -> List[str]:  # noqa: D102
        return []

    def __getattr__(self, attr: str):
        return getattr(self._inner, attr)


@dataclass
class _Ctx:
    """Where sub-steps log, record and report."""

    reporter: Reporter
    log_dir: Path
    results: Path


def _default_ctx(reporter: Optional[Reporter] = None) -> _Ctx:
    return _Ctx(_QuietOk(reporter or get_reporter(None)),
                Path(env_names.get("CI_LOG_DIR", required=True)),
                Path(env_names.get("CI_RESULTS_FILE", required=True)))


def _show(value: object) -> str:
    """Return ``value`` if it is safe to print outside a fence (CR-4), else ``<invalid>``."""
    return value if isinstance(value, str) and _PRINTABLE.match(value) else "<invalid>"


def _discrepancy(what: str, expected: object, actual: object) -> str:
    return f"{what}: expected {_show(expected)} -> actual {_show(actual)}"


def _load_json(path: Path) -> Mapping[str, object]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise BootstrapError(f"cannot read {path.name} ({type(exc).__name__})") from None
    if not isinstance(data, dict):
        raise BootstrapError(f"{path.name} is not a JSON object")
    return data


def _get(data: Mapping[str, object], *keys: str) -> object:
    node: object = data
    for key in keys:
        node = node.get(key) if isinstance(node, dict) else None
    return node


def pinned_baseline(repo_root: Path, pins_file: Path) -> str:
    """Return the vcpkg baseline commit that all three pin sites agree on.

    Args:
        repo_root: Directory holding ``vcpkg.json`` and ``vcpkg-configuration.json``.
        pins_file: The pins data file (``vcpkg.commit``).

    Returns:
        The 40-hex commit id.

    Raises:
        BootstrapError: A file is unreadable, a value is not 40 lowercase hex
            characters, or the values differ. The message is the discrepancy
            line ``vcpkg baseline: expected <a> -> actual <b>``.
    """
    manifest = _get(_load_json(repo_root / "vcpkg.json"), "builtin-baseline")
    config = _get(_load_json(repo_root / "vcpkg-configuration.json"),
                  "default-registry", "baseline")
    pinned = _get(_load_json(pins_file), "vcpkg", "commit")
    expected = manifest if isinstance(manifest, str) and _SHA40.match(manifest) else None
    if expected is None:
        raise BootstrapError(f"vcpkg baseline: expected <40-hex> -> actual {_show(manifest)}")
    for actual in (config, pinned):
        if actual != expected:
            raise BootstrapError(_discrepancy("vcpkg baseline", expected, actual))
    return expected


def _step(ctx: _Ctx, label: str, argv: Sequence[str], timeout_s: float) -> Path:
    """Run one sub-step through ``ci_step``; return its log path or raise (already reported)."""
    result = ci_step.run_step(label, argv, patterns=["vcpkg"], best_effort_reason=None,
                              summary=None, log_dir=ctx.log_dir, reporter=ctx.reporter,
                              results_file=ctx.results, timeout_s=timeout_s)
    if result.exit != 0:
        raise BootstrapError()
    return ci_step.log_path_for(label, ctx.log_dir)


def _first_line(path: Path, limit: int = 4096) -> str:
    try:
        with open(path, "rb") as fh:
            return fh.read(limit).decode("utf-8", errors="replace").strip().splitlines()[0]
    except (OSError, IndexError):
        return ""


def checkout(sha: str, dest: Path, *, ctx: Optional[_Ctx] = None) -> None:
    """Fetch exactly ``sha`` from :data:`VCPKG_URL` into the empty directory ``dest``.

    ``git init`` + ``git fetch --depth 1 <url> <sha>`` + ``git checkout
    --detach FETCH_HEAD``, then ``git rev-parse HEAD`` must equal ``sha``.

    Raises:
        BootstrapError: A git call failed (already reported) or ``HEAD`` differs.
    """
    ctx = ctx or _default_ctx()
    git = ["git", "-C", str(dest)]
    _step(ctx, "vcpkg git init", git + ["init", "-q"], GIT_TIMEOUT_S)
    _step(ctx, "vcpkg git fetch", git + ["fetch", "-q", "--depth", "1", VCPKG_URL, sha],
          GIT_TIMEOUT_S)
    _step(ctx, "vcpkg git checkout", git + ["checkout", "-q", "--detach", "FETCH_HEAD"],
          GIT_TIMEOUT_S)
    log = _step(ctx, "vcpkg git rev-parse", git + ["rev-parse", "HEAD"], GIT_TIMEOUT_S)
    head = _first_line(log)
    if head != sha:
        raise BootstrapError(_discrepancy("vcpkg HEAD", sha, head))


def read_tool_metadata(dest: Path) -> Dict[str, str]:
    """Parse ``scripts/vcpkg-tool-metadata.txt`` (``KEY=VALUE`` lines) of the checkout."""
    path = dest / "scripts" / "vcpkg-tool-metadata.txt"
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        raise BootstrapError("vcpkg-tool-metadata.txt missing from the checkout") from None
    data: Dict[str, str] = {}
    for line in text.splitlines():
        key, sep, value = line.strip().partition("=")
        if sep and key and not key.startswith("#"):
            data[key.strip()] = value.strip()
    return data


def _is_musl() -> bool:
    return any(Path("/lib").glob("ld-musl-*")) if Path("/lib").is_dir() else False


def expected_tool_sha512(dest: Path, pins: Mapping[str, object], *,
                         system: Optional[str] = None, machine: Optional[str] = None,
                         musl: Optional[bool] = None) -> str:
    """Return the SHA-512 the bootstrapped tool binary must have on this platform.

    macOS/Linux mirror ``bootstrap.sh``'s own choice of metadata key; Windows
    uses the pins file (the metadata has no Windows hash).

    Raises:
        BootstrapError: No hash is known for this platform, or the pinned
            Windows tool release no longer matches the checkout's metadata.
    """
    system = system or platform.system()
    machine = (machine or platform.machine()).lower()
    meta = read_tool_metadata(dest)
    if system == "Windows":
        tag = _get(pins, "vcpkg", "tool_release_tag")
        if tag != meta.get("VCPKG_TOOL_RELEASE_TAG"):
            raise BootstrapError(_discrepancy("vcpkg tool release", tag,
                                              meta.get("VCPKG_TOOL_RELEASE_TAG")))
        arch = "arm64" if machine in ("arm64", "aarch64") else "x64"
        value = _get(pins, "vcpkg", "windows_tool_sha512", arch)
    else:
        if system == "Darwin":
            key = "VCPKG_MACOS_SHA"
        elif system == "Linux" and machine in ("x86_64", "amd64"):
            key = "VCPKG_MUSLC_SHA" if (_is_musl() if musl is None else musl) else "VCPKG_GLIBC_SHA"
        elif system == "Linux" and machine in ("aarch64", "arm64") and not (
                _is_musl() if musl is None else musl):
            key = "VCPKG_GLIBC_ARM64_SHA"
        else:
            raise BootstrapError(f"no prebuilt vcpkg tool hash for {_show(system)}/{_show(machine)}")
        value = meta.get(key)
    if not isinstance(value, str) or not _SHA512.match(value.lower()):
        raise BootstrapError("vcpkg tool sha512: no valid expected hash")
    return value.lower()


def sha512_of(path: Path) -> str:
    """Return the hex SHA-512 of ``path`` (streamed)."""
    digest = hashlib.sha512()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tool_path(dest: Path) -> Path:
    """Return where ``bootstrap-vcpkg`` puts the tool binary."""
    return dest / ("vcpkg.exe" if os.name == "nt" else "vcpkg")


def bootstrap(dest: Path, *, pins: Optional[Mapping[str, object]] = None,
              ctx: Optional[_Ctx] = None) -> Path:
    """Run ``bootstrap-vcpkg -disableMetrics`` in ``dest``; return the SHA-512-checked tool.

    Raises:
        BootstrapError: The bootstrap failed (already reported), produced no
            binary, or the binary's SHA-512 differs from the expected value.
    """
    ctx = ctx or _default_ctx()
    pins = pins if pins is not None else _load_json(PINS_FILE)
    script = dest / ("bootstrap-vcpkg.bat" if os.name == "nt" else "bootstrap-vcpkg.sh")
    _step(ctx, "vcpkg bootstrap-vcpkg", [str(script), "-disableMetrics"], BOOTSTRAP_TIMEOUT_S)
    tool = tool_path(dest)
    if not tool.is_file() or tool.is_symlink():
        raise BootstrapError(f"bootstrap produced no {tool.name}")
    expected = expected_tool_sha512(dest, pins)
    actual = sha512_of(tool)
    if actual != expected:
        raise BootstrapError(_discrepancy("vcpkg tool sha512", expected[:16], actual[:16]))
    return tool


def binary_sources(event: str, ref: str, packaging_leg: bool, cache_dir: Path) -> str:
    """Return ``VCPKG_BINARY_SOURCES`` for this run (threat model CR-2).

    - ``tag`` on a packaging leg: exactly ``clear`` (cold build, no cache).
    - ``push`` or ``schedule`` on ``refs/heads/master``
      (:func:`vcpkg_cache_report.save_allowed`): ``clear;files,<dir>,readwrite``.
    - Everything else (PRs, forks, other branches, tags on non-packaging legs,
      manual runs): ``clear;files,<dir>,read``.

    The result always starts with ``clear``, so neither a runner user cache nor
    an inherited value is consulted implicitly.

    Raises:
        ValueError: ``cache_dir`` contains a character vcpkg treats as syntax
            (``,`` ``;`` or a backtick).
    """
    if event == "tag" and packaging_leg:
        return "clear"
    text = str(cache_dir)
    if any(c in text for c in ",;`"):
        raise ValueError("cache directory contains ',', ';' or '`'")
    mode = "readwrite" if vcpkg_cache_report.save_allowed(event, ref) else "read"
    return f"clear;files,{text},{mode}"


def tool_version(tool: Path, *, ctx: Optional[_Ctx] = None) -> str:
    """Return the tool's version token (allowlisted like Task 01's RC4 tokens), else ``?``."""
    ctx = ctx or _default_ctx()
    try:
        log = _step(ctx, "vcpkg version", [str(tool), "--version"], VERSION_TIMEOUT_S)
    except BootstrapError:
        return "?"
    line = _first_line(log)
    # Real output: "... version 2026-07-27-<40-hex tool commit>" (51 characters,
    # over the 40-character token cap), so the commit part is cut to 12 hex.
    match = re.search(r"version (\d{4}-\d{2}-\d{2})(?:-([0-9a-f]{1,40}))?(?:\s|$)", line)
    if match:
        token = match.group(1) + (f"-{match.group(2)[:12]}" if match.group(2) else "")
    else:
        other = re.search(r"version (\S+)", line)
        token = other.group(1) if other else ""
    return token if _VERSION_TOKEN.match(token) else "?"


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="bootstrap_vcpkg.py",
                                     description="Bootstrap vcpkg at the pinned commit.")
    parser.add_argument("--cache-dir", default=None,
                        help=f"files binary-cache dir (default $CI_TEMP_DIR/{CACHE_DIR_NAME})")
    parser.add_argument("--packaging-leg", action="store_true",
                        help="this leg produces release artifacts (tag runs build cold)")
    parser.add_argument("--repo-root", default=None, help="default $CI_WORKSPACE")
    parser.add_argument("--pins", default=str(PINS_FILE))
    parser.add_argument("--reporter", default=None)
    return parser


def main(argv: Sequence[str]) -> int:
    """Bootstrap vcpkg, export its environment into ``os.environ``; return the exit code."""
    args = _parser().parse_args(list(argv))
    try:
        reporter = get_reporter(args.reporter)
    except ValueError as exc:
        ci_step.emit_error(f"bootstrap_vcpkg.py: error: {exc}")
        return env_names.EXIT_ENV_ERROR
    ctx = _default_ctx(reporter)
    temp_dir = Path(env_names.get("CI_TEMP_DIR", required=True))
    repo_root = Path(args.repo_root or env_names.get("CI_WORKSPACE", required=True))
    cache_dir = Path(args.cache_dir) if args.cache_dir else temp_dir / CACHE_DIR_NAME
    start = time.monotonic()
    try:
        sources = binary_sources(env_names.get("CI_EVENT"), env_names.get("CI_REF"),
                                 args.packaging_leg, cache_dir.resolve())
    except ValueError as exc:
        ci_step.emit_error(f"bootstrap_vcpkg.py: error: {exc}")
        return env_names.EXIT_ENV_ERROR
    try:
        pins = _load_json(Path(args.pins))
        sha = pinned_baseline(repo_root, Path(args.pins))
        temp_dir.mkdir(parents=True, exist_ok=True)
        dest = Path(tempfile.mkdtemp(prefix="vcpkg-", dir=str(temp_dir))).resolve()
        checkout(sha, dest, ctx=ctx)
        tool = bootstrap(dest, pins=pins, ctx=ctx)
    except BootstrapError as exc:
        if exc.message:
            elapsed = round(time.monotonic() - start, 1)
            ci_step._emit(reporter.step_fail(LABEL, 1, elapsed, exc.message, []))
            ci_step.append_record(ci_step.StepResult(LABEL, "hard", "fail", 1, elapsed, "",
                                                     exc.message, None), ctx.results)
        return 1
    if sources != "clear":
        cache_dir.mkdir(parents=True, exist_ok=True)
    os.environ["VCPKG_ROOT"] = str(dest)
    os.environ["VCPKG_DISABLE_METRICS"] = "1"
    os.environ["VCPKG_BINARY_SOURCES"] = sources
    version = tool_version(tool, ctx=ctx)
    elapsed = round(time.monotonic() - start, 1)
    summary = f"{sha[:12]}, tool {version}"
    ci_step._emit(reporter.step_ok(LABEL, summary, elapsed))
    ci_step.append_record(ci_step.StepResult(LABEL, "hard", "ok", 0, elapsed, summary, None, None),
                          ctx.results)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - never a traceback (RC2)
        ci_step._emit([f"error: bootstrap_vcpkg.py: internal error ({type(exc).__name__})"])
        sys.exit(70)
