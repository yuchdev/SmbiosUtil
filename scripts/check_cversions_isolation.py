"""Structural guard: keep the GPL ``c-versions/`` references out of every build.

``c-versions/`` holds local, git-ignored clones of the upstream ``dmidecode``
sources (fetched by ``scripts/fetch_upstream_references.py``). Both upstreams
are GPL-2.0-or-later, so this project stays unaffected only if neither is ever
tracked, compiled, or linked. This script turns that constraint into a check
that fails CI (milestone 0008, plan.md R2). It runs four checks and reports
every hit (``file:line: reason``) before exiting non-zero. The ``c-versions``
needle is matched case-insensitively everywhere (macOS and Windows file
systems are case-insensitive, so ``C-Versions/`` names the same directory):

1. **Build references.** Within the build-product directories (``libsmbios/``,
   ``smbios/``, ``sys_interface/``, ``libsmbios_win/``, ``libsmbios_macos/``,
   ``libsmbios_linux/``, ``libsmbios_win_legacy/``,
   ``libsmbios_posix_legacy/``, ``smbios_c/``), plus ``tests/`` and the
   repo-root build files (``CMakeLists.txt``, ``*.cmake``, ``vcpkg.json``,
   ``vcpkg-configuration.json``, ``CMakePresets.json``), the substring
   ``c-versions`` is flagged in:

   - any **non-comment** part of a ``CMakeLists.txt`` / ``*.cmake`` line
     (``#`` line comments and ``#[[ ... ]]`` bracket comments are ignored);
   - any line of a ``vcpkg.json`` / ``vcpkg-configuration.json`` /
     ``CMakePresets.json`` (JSON has no comments);
   - the **``#include`` directive** of a ``*.h``/``*.hpp``/``*.c``/``*.cpp``
     file (trailing comments on that line are ignored);
   - the non-comment part of any **``#define``** line in those files, which
     closes the ``#define H "../c-versions/x.h"`` / ``#include H`` bypass.

   Provenance comments such as ``// Cross-checked against
   c-versions/dmidecode_linux/dmidecode.c`` stay legitimate: a comment cannot
   link GPL code into a binary. Prose (``docs/``, ``CLAUDE.md``,
   ``.claude/``) is out of scope. ``c-versions/`` itself is never scanned and
   never needs to exist.

2. **Submodules.** Fails if ``.gitmodules`` exists at the repo root with any
   ``[submodule ...]`` entry (an entry-less file is only a warning), or if any
   gitlink (mode ``160000``) is tracked.

3. **Tracking.** Fails if ``git ls-files ':(icase)c-versions'`` prints
   anything.

4. **Symlinks.** Fails on any tracked symlink (mode ``120000``, anywhere in
   the repo) whose link target - read from the blob via ``git cat-file -p``
   and from the file on disk - contains ``c-versions``.

Known residual limits (accepted; code review covers them): matching is per
line, so backslash line continuations are not joined; CMake string-splitting
obfuscation (e.g. ``set(A "c-") ... "${A}versions"``) is not detected; and
GPL code copy-pasted into a tracked file is not detected.

Usage::

    python scripts/check_cversions_isolation.py
    python scripts/check_cversions_isolation.py --root /path/to/checkout

Requires ``git`` on ``PATH`` for checks 2-4. Exit status is ``0`` when the
tree is clean, ``1`` on any violation (or when ``git`` cannot answer).
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent

# The git-ignored directory holding the GPL upstream clones.
REFERENCE_DIR = "c-versions"
NEEDLE = "c-versions"

# Build-product directories (see CLAUDE.md), plus tests/ (compiled into the
# ctest executables), scanned recursively.
SCAN_DIRS = (
    "libsmbios",
    "smbios",
    "sys_interface",
    "libsmbios_win",
    "libsmbios_macos",
    "libsmbios_linux",
    "libsmbios_win_legacy",
    "libsmbios_posix_legacy",
    "smbios_c",
    "tests",
)

# Directories never descended into, wherever they appear.
PRUNE_DIRS = frozenset({".git", REFERENCE_DIR})

# Repo-root files scanned non-recursively (the top-level build entry points).
ROOT_JSON_FILES = ("vcpkg.json", "vcpkg-configuration.json", "CMakePresets.json")

JSON_BUILD_FILES = frozenset({"vcpkg.json", "vcpkg-configuration.json"})
C_SUFFIXES = frozenset({".h", ".hpp", ".c", ".cpp"})

_INCLUDE_RE = re.compile(r"^\s*#\s*(?:include_next|include|import)\b(.*)$")
_DEFINE_RE = re.compile(r"^\s*#\s*define\b(.*)$")
_BRACKET_OPEN_RE = re.compile(r"#\[(=*)\[")


@dataclass(frozen=True)
class Violation:
    """One guard hit.

    :ivar path: repo-root-relative POSIX path.
    :ivar line: 1-based line number, or ``None`` for a whole-file hit.
    :ivar reason: human-readable explanation.
    """

    path: str
    line: Optional[int]
    reason: str

    def __str__(self) -> str:
        where = f"{self.path}:{self.line}" if self.line is not None else self.path
        return f"{where}: {self.reason}"


def _has_needle(text: str) -> bool:
    """Return whether ``text`` contains ``c-versions``, ignoring case."""
    return NEEDLE in text.casefold()


def _git(args: list[str], root: Path) -> subprocess.CompletedProcess:
    """Run ``git`` with an argument list (no shell) inside ``root``.

    :param args: arguments after ``git``.
    :param root: repository root to run in.
    :returns: the completed process (never raises on a non-zero exit).
    """
    return subprocess.run(
        ["git", *args],
        cwd=str(root),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )


def _warn(message: str) -> None:
    """Print a warning to stderr."""
    print(f"warning: {message}", file=sys.stderr)


def _rel(path: Path, root: Path) -> str:
    """Return ``path`` relative to ``root`` as a POSIX string."""
    return path.relative_to(root).as_posix()


def _read_lines(path: Path) -> list[str]:
    """Read a text file's lines, tolerating undecodable bytes."""
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def _strip_cmake_line_comment(line: str) -> str:
    """Return ``line`` up to its first ``#`` that is outside a quoted argument.

    Bracket comments are handled by the caller; this only drops a line
    comment, keeping the code before it.
    """
    in_quote = False
    escaped = False
    for index, char in enumerate(line):
        if escaped:
            escaped = False
        elif char == "\\":
            escaped = True
        elif char == '"':
            in_quote = not in_quote
        elif char == "#" and not in_quote:
            return line[:index]
    return line


def check_cmake_file(path: Path, root: Path) -> list[Violation]:
    """Flag ``c-versions`` in any non-comment code of a CMake file.

    :param path: a ``CMakeLists.txt`` or ``*.cmake`` file.
    :param root: repo root, for reporting.
    :returns: one violation per offending line.
    """
    violations: list[Violation] = []
    bracket_close: Optional[str] = None  # e.g. "]==]" while inside #[==[ ... ]==]
    for number, line in enumerate(_read_lines(path), start=1):
        code = ""
        rest = line
        while rest:
            if bracket_close is not None:
                end = rest.find(bracket_close)
                if end < 0:
                    rest = ""
                    break
                rest = rest[end + len(bracket_close):]
                bracket_close = None
                continue
            stripped = _strip_cmake_line_comment(rest)
            if stripped == rest:
                code += rest
                break
            code += stripped
            comment = rest[len(stripped):]
            opener = _BRACKET_OPEN_RE.match(comment)
            if opener is None:
                break  # plain line comment: the rest of the line is ignored
            bracket_close = "]" + opener.group(1) + "]"
            rest = comment[opener.end():]
        if _has_needle(code):
            violations.append(
                Violation(_rel(path, root), number, f"build file references {NEEDLE}/ outside a comment")
            )
    return violations


def check_json_build_file(path: Path, root: Path) -> list[Violation]:
    """Flag ``c-versions`` on any line of a JSON build/dependency manifest.

    :param path: e.g. ``vcpkg.json``.
    :param root: repo root, for reporting.
    :returns: one violation per offending line.
    """
    return [
        Violation(_rel(path, root), number, f"build manifest references {NEEDLE}/")
        for number, line in enumerate(_read_lines(path), start=1)
        if _has_needle(line)
    ]


def _include_target(directive_rest: str) -> str:
    """Return the part of an ``#include`` line that names what is included.

    For ``<...>``/``"..."`` forms that is the header name; for a macro form it
    is the rest of the line minus any trailing ``//`` or ``/*`` comment.
    """
    rest = directive_rest.strip()
    if rest[:1] in ('"', "<"):
        closer = '"' if rest[0] == '"' else ">"
        end = rest.find(closer, 1)
        return rest if end < 0 else rest[: end + 1]
    for marker in ("//", "/*"):
        cut = rest.find(marker)
        if cut >= 0:
            rest = rest[:cut]
    return rest


def _strip_c_comment(text: str) -> str:
    """Return ``text`` up to its first ``//`` or ``/*`` outside a string literal."""
    quote: Optional[str] = None
    escaped = False
    for index, char in enumerate(text):
        if escaped:
            escaped = False
        elif char == "\\":
            escaped = True
        elif quote is not None:
            if char == quote:
                quote = None
        elif char in ('"', "'"):
            quote = char
        elif text.startswith(("//", "/*"), index):
            return text[:index]
    return text


def check_c_source(path: Path, root: Path) -> list[Violation]:
    """Flag ``c-versions`` in the ``#include``/``#define`` directives of a C/C++ file.

    Only the directive itself is inspected: comments (``//``, ``///``,
    ``/* ... */`` and their `` * `` continuation lines), including a trailing
    comment on the directive line, never match. ``#define`` lines are checked
    so a macro cannot smuggle the path into a later ``#include MACRO``.

    :param path: a ``*.h``/``*.hpp``/``*.c``/``*.cpp`` file.
    :param root: repo root, for reporting.
    :returns: one violation per offending directive.
    """
    violations: list[Violation] = []
    for number, line in enumerate(_read_lines(path), start=1):
        match = _INCLUDE_RE.match(line)
        if match and _has_needle(_include_target(match.group(1))):
            violations.append(
                Violation(_rel(path, root), number, f"#include of a {NEEDLE}/ (GPL) source")
            )
            continue
        match = _DEFINE_RE.match(line)
        if match and _has_needle(_strip_c_comment(match.group(1))):
            violations.append(
                Violation(_rel(path, root), number, f"#define referencing a {NEEDLE}/ (GPL) source")
            )
    return violations


def _iter_scope_files(root: Path) -> Iterator[Path]:
    """Yield every file under the scanned directories, pruning ``c-versions/``."""
    for name in SCAN_DIRS:
        top = root / name
        if not top.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(top):
            dirnames[:] = sorted(d for d in dirnames if d.casefold() not in PRUNE_DIRS)
            for filename in sorted(filenames):
                yield Path(dirpath) / filename


def check_build_references(root: Path) -> list[Violation]:
    """Run the build-reference check over the whole scan scope.

    :param root: repo root.
    :returns: every violation found.
    """
    violations: list[Violation] = []
    candidates = list(_iter_scope_files(root))
    candidates += sorted(p for p in root.glob("*.cmake") if p.is_file())
    candidates += [root / name for name in ("CMakeLists.txt", *ROOT_JSON_FILES)]
    for path in candidates:
        if not path.is_file():
            continue
        name = path.name
        if name == "CMakeLists.txt" or name.endswith(".cmake"):
            violations += check_cmake_file(path, root)
        elif name in JSON_BUILD_FILES or (path.parent == root and name in ROOT_JSON_FILES):
            violations += check_json_build_file(path, root)
        elif path.suffix in C_SUFFIXES:
            violations += check_c_source(path, root)
    return violations


def check_submodules(root: Path) -> list[Violation]:
    """Fail on any ``.gitmodules`` entry or tracked gitlink.

    :param root: repo root.
    :returns: every violation found (an entry-less ``.gitmodules`` only warns).
    :raises RuntimeError: if ``git ls-files`` fails.
    """
    violations: list[Violation] = []
    gitmodules = root / ".gitmodules"
    if gitmodules.is_file():
        entries = [
            (number, line.strip())
            for number, line in enumerate(_read_lines(gitmodules), start=1)
            if re.match(r"\s*\[\s*submodule\b", line)
        ]
        for number, line in entries:
            violations.append(
                Violation(".gitmodules", number, f"submodule entry {line} - no submodules are allowed (GPL isolation)")
            )
        if not entries:
            _warn(".gitmodules exists but has no [submodule] entries; delete it")

    proc = _git(["ls-files", "--stage"], root)
    if proc.returncode != 0:
        raise RuntimeError(f"git ls-files --stage failed: {proc.stderr.strip() or proc.returncode}")
    for record in proc.stdout.splitlines():
        meta, _, path = record.partition("\t")
        if meta.split(" ", 1)[0] == "160000":
            violations.append(Violation(path, None, "tracked gitlink (submodule) - no submodules are allowed"))
    return violations


def check_symlinks(root: Path) -> list[Violation]:
    """Fail on any tracked symlink whose target names ``c-versions``.

    The target is read both from the index blob (``git cat-file -p``) and,
    when present, from the link on disk, so neither a committed nor a
    locally-retargeted link slips through.

    :param root: repo root.
    :returns: one violation per offending symlink.
    :raises RuntimeError: if ``git ls-files`` fails.
    """
    proc = _git(["ls-files", "--stage"], root)
    if proc.returncode != 0:
        raise RuntimeError(f"git ls-files --stage failed: {proc.stderr.strip() or proc.returncode}")
    violations: list[Violation] = []
    for record in proc.stdout.splitlines():
        meta, _, path = record.partition("\t")
        fields = meta.split()
        if len(fields) < 2 or fields[0] != "120000":
            continue
        targets: list[str] = []
        blob = _git(["cat-file", "-p", fields[1]], root)
        if blob.returncode == 0:
            targets.append(blob.stdout)
        on_disk = root / path
        if on_disk.is_symlink():
            try:
                targets.append(os.readlink(on_disk))
            except OSError:
                pass
        if any(_has_needle(target) for target in targets):
            violations.append(
                Violation(path, None, f"tracked symlink points into {REFERENCE_DIR}/ (GPL) - never link to it")
            )
    return violations


def check_tracking(root: Path) -> list[Violation]:
    """Fail on any tracked file under ``c-versions/`` (any letter case).

    :param root: repo root.
    :returns: one violation per tracked path.
    :raises RuntimeError: if ``git ls-files`` fails.
    """
    proc = _git(["ls-files", "--", f":(icase){REFERENCE_DIR}"], root)
    if proc.returncode != 0:
        raise RuntimeError(f"git ls-files {REFERENCE_DIR} failed: {proc.stderr.strip() or proc.returncode}")
    return [
        Violation(path, None, f"tracked under {REFERENCE_DIR}/ - GPL upstream code must never be committed")
        for path in proc.stdout.splitlines()
        if path
    ]


def main(argv: Optional[list[str]] = None) -> int:
    """Command-line entry point.

    :param argv: arguments (default: ``sys.argv[1:]``).
    :returns: ``0`` if clean, ``1`` on any violation or git failure.
    """
    parser = argparse.ArgumentParser(
        description="Fail if c-versions/ (GPL upstream references) is built, included, linked, or tracked."
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=REPO_ROOT,
        help="Repository root to check (default: this script's checkout).",
    )
    args = parser.parse_args(argv)
    root = args.root.resolve()

    violations = check_build_references(root)
    try:
        violations += check_submodules(root)
        violations += check_tracking(root)
        violations += check_symlinks(root)
    except (OSError, RuntimeError) as exc:  # e.g. git not on PATH, not a repo
        print(f"error: {exc}", file=sys.stderr)
        for violation in violations:
            print(violation)
        return 1

    for violation in violations:
        print(violation)
    sys.stdout.flush()
    if violations:
        print(
            f"c-versions isolation check FAILED: {len(violations)} violation(s). "
            f"{REFERENCE_DIR}/ is GPL reference material - it must stay git-ignored "
            "and must never be built, #included, or tracked (see "
            "docs/roadmap/0008-upstream-reference-submodules-and-monitoring-agent/plan.md R2).",
            file=sys.stderr,
        )
        return 1
    print("c-versions isolation check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
