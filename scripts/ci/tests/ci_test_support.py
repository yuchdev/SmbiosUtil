"""Shared helpers for the ``scripts/ci`` test harness (milestone 0014, Story 01.0 Task 03).

Not a test module (no ``test_`` prefix), so ``unittest discover`` does not
collect it. Everything here is stdlib only and touches no network, no host
cmake/ctest and no firmware data: child processes are ``sys.executable``
one-liners or tiny Python stubs written into a temporary directory.
"""

from __future__ import annotations

import json
import os
import stat
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Sequence

TESTS_DIR = Path(__file__).resolve().parent
CI_DIR = TESTS_DIR.parent
FIXTURES = TESTS_DIR / "fixtures"
CI_STEP = CI_DIR / "ci_step.py"

if str(CI_DIR) not in sys.path:
    sys.path.insert(0, str(CI_DIR))

#: RC1 ceilings, restated here on purpose: the tests pin the contract values
#: from plan.md, not whatever constants ci_step.py happens to hold.
RC1_MAX_DIAG = 20
RC1_MAX_TAIL = 40
RC1_LINE_WIDTH = 200
RC1_MAX_LINES = 64
RC1_MAX_BYTES = 16 * 1024
RC1_ANNOTATION_MAX = 160

#: Child that copies a file to stdout byte-for-byte, then exits with a code.
CAT_CHILD = ("import shutil, sys\n"
             "with open(sys.argv[1], 'rb') as fh:\n"
             "    shutil.copyfileobj(fh, sys.stdout.buffer)\n"
             "sys.stdout.flush()\n"
             "sys.exit(int(sys.argv[2]))\n")

#: RC3 record keys (plan.md RC3).
RC3_KEYS = {"label", "mode", "outcome", "exit", "elapsed_s", "summary",
            "first_diagnostic", "counts"}


def cat_argv(path: Path, rc: int) -> List[str]:
    """Return argv for a child that prints ``path`` and exits ``rc``."""
    return [sys.executable, "-c", CAT_CHILD, str(path), str(rc)]


def neutral_env(tmp: Path, **extra: str) -> Dict[str, str]:
    """Return a child environment with every ``CI_*`` name cleared, logs/records under ``tmp``."""
    env = {k: v for k, v in os.environ.items() if not k.startswith("CI_") and k != "CI"}
    env["CI_LOG_DIR"] = str(tmp / "logs")
    env["CI_RESULTS_FILE"] = str(tmp / "results.jsonl")
    env["CI_TEMP_DIR"] = str(tmp)
    env.update(extra)
    return env


class CliRun(NamedTuple):
    """Outcome of one ``ci_step.py`` command-line invocation."""

    rc: int
    lines: List[str]
    raw: bytes
    stderr: str
    records: List[dict]


def run_cli(tmp: Path, args: Sequence[str], *, env: Optional[Dict[str, str]] = None,
            cwd: Optional[Path] = None, timeout: float = 60) -> CliRun:
    """Run ``python -I ci_step.py <args>`` and collect stdout lines and RC3 records."""
    env = env if env is not None else neutral_env(tmp)
    proc = subprocess.run([sys.executable, "-I", str(CI_STEP), *args],
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          stdin=subprocess.DEVNULL, env=env, cwd=str(cwd or tmp),
                          timeout=timeout)
    results = Path(env.get("CI_RESULTS_FILE", str(tmp / "results.jsonl")))
    records = []
    if results.exists():
        records = [json.loads(x) for x in results.read_text(encoding="ascii").splitlines()]
    text = proc.stdout.decode("utf-8", errors="strict")
    return CliRun(proc.returncode, text.splitlines(), proc.stdout,
                  proc.stderr.decode("utf-8", errors="replace"), records)


def write_stub(directory: Path, name: str, body: str) -> Path:
    """Write a tiny Python stub command ``name`` into ``directory``; return its stub script.

    On POSIX a ``/bin/sh`` trampoline called ``name`` execs ``sys.executable``
    on ``<name>_stub.py`` (a shebang to ``sys.executable`` would break on a
    path with spaces). On Windows ``subprocess`` cannot start a script, so
    callers use :func:`stub_argv` to get a runnable argv.
    """
    directory.mkdir(parents=True, exist_ok=True)
    script = directory / f"{name}_stub.py"
    script.write_text(body, encoding="utf-8")
    trampoline = directory / name
    trampoline.write_text(
        "#!/bin/sh\n"
        f"exec \"{Path(sys.executable).as_posix()}\" \"{script.as_posix()}\" \"$@\"\n",
        encoding="utf-8", newline="\n")
    trampoline.chmod(trampoline.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    return script


def stub_argv(directory: Path, name: str) -> List[str]:
    """Return argv that runs stub ``name`` by name via ``PATH`` (explicitly on Windows)."""
    if os.name == "nt":
        return [sys.executable, str(directory / f"{name}_stub.py")]
    return [name]


def cat_stub_body(fixture: Path, rc: int) -> str:
    """Return a stub body that prints ``fixture`` and exits ``rc``."""
    return ("import shutil, sys\n"
            f"with open({str(fixture)!r}, 'rb') as fh:\n"
            "    shutil.copyfileobj(fh, sys.stdout.buffer)\n"
            "sys.stdout.flush()\n"
            f"sys.exit({rc})\n")


def path_with(directory: Path) -> str:
    """Return ``PATH`` with ``directory`` first, so a stub shadows any host tool."""
    return str(directory) + os.pathsep + os.environ.get("PATH", "")


def rc1_bytes(lines: Sequence[str]) -> int:
    """Return the printed size of ``lines`` in bytes (newline-joined, UTF-8)."""
    return len("\n".join(lines).encode("utf-8"))
