#!/usr/bin/env python3
"""Print the RC4 version header (milestone 0014, Story 02.0 Task 01).

The first project-owned step of every job. It prints one line of at most 240
characters::

    env: <job>/<leg> image=<CI_IMAGE> | cmake <v> | ctest <v> | ninja <v|-> | cxx <id> <v> | python <v> | vcpkg <sha12> | cache <exact|partial|miss|->

and, on a CI platform, also writes it as the first line of the job summary.
Both go through the reporter backend (``Reporter.info`` / ``job_summary``,
ADR 0005); this script never prints directly. It hands the same line to later
steps (RC3 records, Story 04.0's ``job_summary.py``) as the file
:data:`ENV_LINE_FILE` under ``$CI_TEMP_DIR``: created mode 0600, opened with
``O_NOFOLLOW`` (threat model CR-11), never through a platform env file.

Rules:

- Each tool probe has a 10 s timeout and a bounded read of its output. A tool
  that is missing, cannot start or hangs gives ``-``; a tool whose output has
  no recognisable version gives ``?``. This step is informational and always
  exits 0.
- A probed token is printed only if it matches :data:`VERSION_TOKEN`
  (``^[0-9A-Za-z.+_-]{1,40}$``), else ``?`` (threat model CR-4), so tool output
  can never carry a workflow command or service message into the log.
- ``vcpkg <sha12>`` is the pinned commit from ``pins.json`` (equal to the
  manifest baseline; Task 02's ``bootstrap_vcpkg.py`` fails hard on a
  mismatch). ``cache`` is ``-`` unless ``--cache`` names the restore result.
- Python 3.9 or later is required; below that the script exits 2.

Usage::

    python3 scripts/ci/print_env.py [--job <job>] [--leg <leg>]
            [--cache exact|partial|miss] [--reporter github|teamcity|plain]

``--job``/``--leg`` default to ``$CI_JOB``/``$CI_LEG`` (``local`` when unset).
Python 3 standard library only.
"""

from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import argparse  # noqa: E402
import json  # noqa: E402
import platform  # noqa: E402
import re  # noqa: E402
import subprocess  # noqa: E402
import tempfile  # noqa: E402
from pathlib import Path  # noqa: E402
from typing import Mapping, Optional, Sequence, Tuple  # noqa: E402

import ci_step  # noqa: E402
import env_names  # noqa: E402
from reporters import child_environment, get_reporter  # noqa: E402

#: RC4 ceiling (plan.md).
LINE_MAX = 240
#: CR-4 allowlist for a probed version token.
VERSION_TOKEN = re.compile(r"^[0-9A-Za-z.+_-]{1,40}$")
#: Allowlist for ``<job>``/``<leg>`` (they come from the workflow, not a tool).
NAME_TOKEN = re.compile(r"^[0-9A-Za-z._-]{1,40}$")
#: Allowlist for ``image=`` after whitespace is folded to ``/``.
IMAGE_TOKEN = re.compile(r"^[0-9A-Za-z.+_/-]{1,60}$")
#: Bytes of probe output read; a version banner is far shorter.
PROBE_READ_MAX = 64 * 1024
#: Hand-off file name under ``$CI_TEMP_DIR`` (one line, the RC4 header).
ENV_LINE_FILE = "ci-env-line.txt"
#: The pins data file (ADR 0005 D3).
PINS_FILE = Path(_HERE) / "pins.json"
#: Values ``--cache`` accepts (Task 02's cache-health result).
CACHE_STATES = ("exact", "partial", "miss")
#: Default per-probe timeout in seconds (Task 01 spec).
PROBE_TIMEOUT_S = 10.0
#: vswhere's fixed install path on the hosted Windows images (VS installer).
_VSWHERE = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
_SHA40 = re.compile(r"^[0-9a-f]{40}$")


def _run(argv: Sequence[str], timeout_s: float) -> Optional[str]:
    """Run ``argv`` without a shell; return its first output bytes as text, or ``None``.

    stdout+stderr go to an anonymous temporary file, so memory stays bounded
    whatever the child prints; only :data:`PROBE_READ_MAX` bytes are read back.
    ``None`` means the tool is missing, could not start, or did not finish
    within ``timeout_s`` (it is killed).
    """
    with tempfile.TemporaryFile() as out:
        try:
            proc = subprocess.Popen(list(argv), stdin=subprocess.DEVNULL, stdout=out,
                                    stderr=subprocess.STDOUT,
                                    env=child_environment(os.environ))
        except (OSError, ValueError):
            return None
        try:
            proc.wait(timeout=timeout_s)
        except subprocess.TimeoutExpired:
            proc.kill()
            try:
                proc.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                pass
            return None
        out.seek(0)
        return out.read(PROBE_READ_MAX).decode("utf-8", errors="replace")


def _token(value: Optional[str]) -> str:
    """Return ``value`` if it passes :data:`VERSION_TOKEN`, else ``?``."""
    return value if value is not None and VERSION_TOKEN.match(value) else "?"


def probe(argv: Sequence[str], pattern: str, timeout_s: float = PROBE_TIMEOUT_S) -> str:
    """Return the tool's version, ``-`` if it is missing or hangs, ``?`` if unrecognised.

    Args:
        argv: Command to run (no shell), e.g. ``["cmake", "--version"]``.
        pattern: Regex searched in the output (multi-line); group 1 is the version.
        timeout_s: Kill the probe after this many seconds.

    Returns:
        A token matching :data:`VERSION_TOKEN`, ``-`` or ``?`` - never raw tool text.
    """
    text = _run(argv, timeout_s)
    if text is None:
        return "-"
    match = re.search(pattern, text, re.MULTILINE)
    return _token(match.group(1) if match else None)


def probe_cxx(timeout_s: float = PROBE_TIMEOUT_S) -> Tuple[str, str]:
    """Return ``(id, version)`` of the default C++ compiler, ``("-", "-")`` if none.

    POSIX: ``c++ --version``, classified as ``appleclang``, ``clang`` or ``gcc``.
    Windows: ``cl`` when a developer prompt put it on ``PATH``; otherwise the
    Visual Studio 2022 instance ``vswhere`` reports (``vs2022 <installationVersion>``),
    which is the toolset the ``windows`` preset's generator uses.
    """
    if os.name == "nt":
        text = _run(["cl"], timeout_s)
        if text is not None:
            match = re.search(r"Version (\d[\w.]*)", text)
            return "msvc", _token(match.group(1) if match else None)
        text = _run([_VSWHERE, "-products", "*", "-version", "[17.0,18.0)",
                     "-property", "installationVersion"], timeout_s)
        if text is None:
            return "-", "-"
        match = re.search(r"^\s*(\S+)", text)
        return "vs2022", _token(match.group(1) if match else None)
    text = _run(["c++", "--version"], timeout_s)
    if text is None:
        return "-", "-"
    for cxx_id, pattern in (("appleclang", r"Apple clang version (\S+)"),
                            ("clang", r"clang version (\S+)"),
                            ("gcc", r"^\S*(?:g\+\+|c\+\+)\S* \(.*\) (\S+)")):
        match = re.search(pattern, text, re.MULTILINE)
        if match:
            return cxx_id, _token(match.group(1))
    return "?", "?"


def load_pins(path: Path = PINS_FILE) -> Mapping[str, object]:
    """Return the parsed pins file; an unreadable or malformed file gives ``{}``."""
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    return data if isinstance(data, dict) else {}


def vcpkg_sha12(pins: Mapping[str, object]) -> str:
    """Return the first 12 characters of the pinned vcpkg commit, ``?`` if invalid."""
    vcpkg = pins.get("vcpkg")
    commit = vcpkg.get("commit") if isinstance(vcpkg, dict) else None
    return commit[:12] if isinstance(commit, str) and _SHA40.match(commit) else "?"


def _name(value: str) -> str:
    return value if NAME_TOKEN.match(value) else "?"


def _image(value: str) -> str:
    if not value.strip():
        return "-"
    folded = "/".join(value.split())
    return folded if IMAGE_TOKEN.match(folded) else "?"


def env_line(job: str, leg: str, *, image: Optional[str] = None, cache: str = "-",
             pins: Optional[Mapping[str, object]] = None,
             timeout_s: float = PROBE_TIMEOUT_S) -> str:
    """Return the RC4 header line (at most :data:`LINE_MAX` characters).

    Args:
        job: Stage job name; anything outside :data:`NAME_TOKEN` prints ``?``.
        leg: Leg name; same rule.
        image: ``image=`` value; ``None`` reads ``$CI_IMAGE``.
        cache: ``exact``/``partial``/``miss``, or ``-`` before any restore.
        pins: Parsed pins file; ``None`` loads :data:`PINS_FILE`.
        timeout_s: Per-probe timeout.
    """
    image_value = env_names.get("CI_IMAGE") if image is None else image
    pins_value = load_pins() if pins is None else pins
    cmake = probe(["cmake", "--version"], r"cmake version (\S+)", timeout_s)
    ctest = probe(["ctest", "--version"], r"ctest version (\S+)", timeout_s)
    ninja = probe(["ninja", "--version"], r"^\s*(\S+)", timeout_s)
    cxx_id, cxx_ver = probe_cxx(timeout_s)
    fields = [
        f"env: {_name(job)}/{_name(leg)} image={_image(image_value)}",
        f"cmake {cmake}",
        f"ctest {ctest}",
        f"ninja {ninja}",
        f"cxx {cxx_id} {cxx_ver}",
        f"python {_token(platform.python_version())}",
        f"vcpkg {vcpkg_sha12(pins_value)}",
        f"cache {cache if cache in CACHE_STATES else '-'}",
    ]
    return " | ".join(fields)[:LINE_MAX]


def write_env_line(line: str, temp_dir: Path) -> Path:
    """Write ``line`` to ``<temp_dir>/ci-env-line.txt``: mode 0600, never through a symlink.

    Raises:
        OSError: The file cannot be written; a symlink at the path fails
            (``ELOOP`` on POSIX) and its target is left unchanged.
    """
    path = temp_dir / ENV_LINE_FILE
    if path.is_symlink():  # O_NOFOLLOW does not exist on Windows; refuse there too
        raise OSError(f"refusing symlink {ENV_LINE_FILE}")
    flags = (os.O_WRONLY | os.O_CREAT | os.O_TRUNC
             | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_BINARY", 0))
    fd = os.open(str(path), flags, 0o600)
    try:
        if hasattr(os, "fchmod"):
            os.fchmod(fd, 0o600)  # the file may have existed with a wider mode
        os.write(fd, (line + "\n").encode("utf-8"))
    finally:
        os.close(fd)
    return path


def python_supported(version: Tuple[int, ...] = tuple(sys.version_info[:2])) -> bool:
    """Return whether ``version`` is at least 3.9 (the stdlib floor of ``scripts/ci``)."""
    return tuple(version[:2]) >= (3, 9)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="print_env.py", description="Print the RC4 header.")
    parser.add_argument("--job", default=None)
    parser.add_argument("--leg", default=None)
    parser.add_argument("--cache", choices=CACHE_STATES, default="-")
    parser.add_argument("--reporter", default=None)
    return parser


def main(argv: Sequence[str]) -> int:
    """Print the RC4 line, publish it as the summary's first line, hand it off; return 0."""
    args = _parser().parse_args(list(argv))
    try:
        reporter = get_reporter(args.reporter)
    except ValueError:
        reporter = get_reporter("plain")  # informational step: never fail on this
    job = args.job or env_names.get("CI_JOB") or "local"
    leg = args.leg or env_names.get("CI_LEG") or "local"
    line = env_line(job, leg, cache=args.cache)
    lines = reporter.info(line)
    try:
        write_env_line(line, Path(env_names.get("CI_TEMP_DIR", required=True)))
    except OSError as exc:
        lines += reporter.info(f"info: print_env: env line not handed off "
                               f"({type(exc).__name__})")
    ci_step._emit(lines)  # the runner's single stdout writer (CR-4 single emitter)
    if reporter.name != "plain":  # plain's summary target is stdout, already printed
        reporter.job_summary(lines[0])
    return 0


if __name__ == "__main__":
    if not python_supported():
        ci_step.emit_error("error: print_env.py needs Python 3.9 or later")
        sys.exit(env_names.EXIT_ENV_ERROR)
    try:
        sys.exit(main(sys.argv[1:]))
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001 - informational step: one line, exit 0
        ci_step._emit([f"info: print_env: internal error ({type(exc).__name__})"])
        sys.exit(0)
