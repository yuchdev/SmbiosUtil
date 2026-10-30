"""Insert standardized Doxygen documentation comments onto GoogleTest test cases.

For every ``TEST``/``TEST_F``/``TEST_P``/``TYPED_TEST``/``TYPED_TEST_P`` macro
invocation found under a target directory, generates a Doxygen comment that
classifies the test as one of ``Unit``, ``Mock``, ``Integration``, or ``E2E``
and fills a fixed narrative template (Scenario / Boundaries / On-failure-first-
check) derived mechanically from the test's suite name, test name, and body -
no test logic is read for "meaning", only scanned for structural signals
(mocking, subprocess/end-to-end runners, directory placement).

There is no C++ equivalent of Python's ``ast`` module in the stdlib, so this
codemod is regex- and brace-counting-based rather than AST-based. It is a
best-effort heuristic, same spirit as ``document_tests.py`` in the ``python``
preset, not a C++ parser: unusual formatting (a macro invocation split across
many lines, braces inside a raw string literal used as a test-body delimiter)
can confuse it. When in doubt it flags a test ``ambiguous`` rather than
guessing silently - see the ``document-tests`` skill for how those are
hand-resolved.

Classification heuristic (path/filename-first, body-refined):

- path contains an ``e2e`` directory, or the filename ends ``_e2e_test.cpp``/
  ``_e2e_test.cc`` -> ``E2E`` (end-to-end suite by convention).
- else path contains an ``integration`` directory, or the filename ends
  ``_integration_test.cpp``/``_integration_test.cc``/``IT.cpp``/``IT.cc``
  (Maven-Failsafe-style convention some C++ repos borrow):
    - body uses an end-to-end marker (a spawned subprocess, a real HTTP/gRPC
      client, a socket) -> ``E2E``
    - else -> ``Integration``
- else path contains a ``unit``/``test``/``tests`` directory (or unrecognised
  layout):
    - body uses an end-to-end marker -> ``E2E``
    - body uses a GoogleMock marker -> ``Mock``
    - else -> ``Unit``
    - tests in an unrecognised directory are additionally flagged
      ``ambiguous`` for human/agent review, since no directory convention
      backs the guess.

A generated Doxygen comment is recognisable by its first content line matching
``r"^\\[(Unit|Mock|Integration|E2E)\\] .+: verifies .+\\.$"``. Re-running this
script re-generates (and overwrites) only comments matching that pattern, so
it is safe to run repeatedly as tests change. A pre-existing hand-written
comment that does NOT match the pattern is left alone and reported as
skipped, unless ``--force`` is passed.

Usage::

    python scripts/document_tests.py                     # scan the whole repo
    python scripts/document_tests.py tests/unit           # scope to a directory
    python scripts/document_tests.py --check              # report only, no writes
    python scripts/document_tests.py --force              # overwrite hand-written comments too

Exit status is ``1`` in ``--check`` mode when any test would change, else ``0``.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent

DEFAULT_TARGET = "."

# Directories that never hold real test sources - skip descending into them.
EXCLUDED_DIRS = {
    ".git",
    ".idea",
    "build",
    "out",
    "_deps",
    "node_modules",
    "vcpkg_installed",
    ".cache",
}
# Prefixes for generated out-of-source build directories (CLion's convention
# among others), which don't fit a fixed-name exclusion set.
EXCLUDED_DIR_PREFIXES = ("cmake-build",)

CLASSIFICATIONS = ("Unit", "Mock", "Integration", "E2E")

_GENERATED_TITLE_RE = re.compile(r"^\[(Unit|Mock|Integration|E2E)\] .+: verifies .+\.$")

_E2E_BODY_MARKERS = (
    "std::system(",
    " system(",
    "popen(",
    "fork(",
    "execve(",
    "boost::process",
    "subprocess::",
    "curl_easy_",
    "httplib::client",
    "grpc::createchannel",
    "::socket(",
)
_MOCK_BODY_MARKERS = (
    "mock_method",
    "expect_call(",
    "nicemock<",
    "strictmock<",
    "on_call(",
    "::testing::mock",
    "returnref(",
    "willonce(",
    "willrepeatedly(",
)

_TEST_START_RE = re.compile(r"^([ \t]*)\b(TEST_P|TEST_F|TYPED_TEST_P|TYPED_TEST|TEST)\s*\(")
_MACRO_RE = re.compile(
    r"\b(TEST_P|TEST_F|TYPED_TEST_P|TYPED_TEST|TEST)\s*\(\s*([A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*\)",
    re.S,
)


def _display_path(file_path: Path) -> str:
    try:
        return str(file_path.relative_to(REPO_ROOT))
    except ValueError:
        return str(file_path)


@dataclass
class TestCase:
    """One discovered GoogleTest macro invocation and its rendered Doxygen comment."""

    file_path: Path
    qualname: str
    test_name: str
    classification: str
    ambiguous: bool
    insert_line: int  # 0-indexed line to insert before (the top of the macro invocation)
    delete_start: Optional[int]  # 0-indexed inclusive start of an existing generated comment
    delete_end: Optional[int]  # 0-indexed inclusive end of an existing generated comment
    skip_custom_docstring: bool
    indent: str
    rendered: str = field(default="")


def _humanize(identifier: str) -> str:
    name = re.sub(r"^(test_?|Test)", "", identifier)
    name = name.replace("_", " ")
    words: list[str] = []
    for chunk in name.split():
        words.extend(re.findall(r"[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z0-9]+|[A-Z]+", chunk))
    return " ".join(w.lower() for w in words) if words else name.lower()


def _humanize_suite(suite_name: str) -> str:
    name = re.sub(r"(Tests?|TestSuite|TestCase|Fixture|IT)$", "", suite_name)
    words = re.findall(r"[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z0-9]+|[A-Z]+", name)
    return " ".join(w.lower() for w in words) if words else suite_name.lower()


def _context_label(file_path: Path, suite_name: str) -> str:
    return _humanize_suite(suite_name) if suite_name else _humanize_suite(file_path.stem)


def _params_line(macro: str) -> str:
    if macro == "TEST_P":
        return "GetParam()"
    if macro == "TYPED_TEST_P":
        return "TypeParam"
    return "none"


def _extract_body(lines: list[str], brace_line: int) -> str:
    """Brace-count from the first ``{`` at/after ``brace_line`` to its match.

    Ignores string/char literal and comment contents on a best-effort basis
    (not a full lexer) - good enough to keep an unrelated ``}`` inside a log
    message or a raw string literal from truncating the scan early in the
    common case.
    """
    depth = 0
    started = False
    in_line_comment = False
    in_block_comment = False
    in_string = False
    in_char = False
    collected: list[str] = []

    for line in lines[brace_line:]:
        in_line_comment = False
        i = 0
        buf = []
        while i < len(line):
            ch = line[i]
            nxt = line[i + 1] if i + 1 < len(line) else ""
            if in_line_comment:
                break
            if in_block_comment:
                if ch == "*" and nxt == "/":
                    in_block_comment = False
                    i += 2
                    continue
                i += 1
                continue
            if in_string:
                buf.append(ch)
                if ch == "\\":
                    i += 2
                    continue
                if ch == '"':
                    in_string = False
                i += 1
                continue
            if in_char:
                buf.append(ch)
                if ch == "\\":
                    i += 2
                    continue
                if ch == "'":
                    in_char = False
                i += 1
                continue
            if ch == "/" and nxt == "/":
                in_line_comment = True
                break
            if ch == "/" and nxt == "*":
                in_block_comment = True
                i += 2
                continue
            if ch == '"':
                in_string = True
                buf.append(ch)
                i += 1
                continue
            if ch == "'":
                in_char = True
                buf.append(ch)
                i += 1
                continue
            if ch == "{":
                depth += 1
                started = True
            elif ch == "}":
                depth -= 1
            buf.append(ch)
            i += 1
            if started and depth == 0:
                collected.append("".join(buf))
                return "\n".join(collected).lower()
        collected.append("".join(buf))
    return "\n".join(collected).lower()


def _classify(file_path: Path, body_text: str) -> tuple[str, bool]:
    parts = {p.lower() for p in file_path.parts}
    stem = file_path.stem
    has_e2e_marker = any(marker in body_text for marker in _E2E_BODY_MARKERS)
    has_mock_marker = any(marker in body_text for marker in _MOCK_BODY_MARKERS)

    if "e2e" in parts or stem.endswith(("_e2e_test", "E2ETest")):
        return "E2E", False

    if "integration" in parts or stem.endswith(("_integration_test", "IT")):
        if has_e2e_marker:
            return "E2E", False
        return "Integration", False

    if parts & {"unit", "test", "tests"}:
        if has_e2e_marker:
            return "E2E", False
        if has_mock_marker:
            return "Mock", False
        return "Unit", False

    # Unrecognised directory layout: best-effort cascade, flagged for review.
    if has_e2e_marker:
        return "E2E", True
    if has_mock_marker:
        return "Mock", True
    return "Unit", True


def _render_doc_comment(indent: str, classification: str, context_label: str, test_name: str, params: str) -> str:
    subject = _humanize(test_name)

    lines = [
        f"{indent}/**",
        f"{indent} * [{classification}] {context_label}: verifies {subject}.",
        f"{indent} *",
        f"{indent} * Scenario:",
        f"{indent} *   - Given {params}",
        f"{indent} *   - When {test_name} executes the target flow",
        f"{indent} *   - Then the expected outcome for {subject} is confirmed",
        f"{indent} *",
        f"{indent} * Boundaries:",
        f"{indent} *   - Focus: {subject}",
        f"{indent} *   - Fixtures/params: {params}",
        f"{indent} *   - Scope: assertions and setup in this test case only",
        f"{indent} *",
        f"{indent} * On failure, first check:",
        f"{indent} *   - Assertion details tied to {subject}",
        f"{indent} *   - Fixture or mock setup used by this test",
        f"{indent} *   - Recent changes in code paths exercised by {context_label}",
        f"{indent} */",
    ]
    return "\n".join(lines)


def _existing_doc_comment_range(lines: list[str], top: int) -> tuple[Optional[int], Optional[int], Optional[str]]:
    end = top - 1
    while end >= 0 and not lines[end].strip():
        end -= 1
    if end < 0 or not lines[end].strip().endswith("*/"):
        return None, None, None

    start = end
    while start >= 0 and "/**" not in lines[start]:
        start -= 1
        if start < top - 200:  # runaway guard for a malformed/missing opener
            return None, None, None
    if start < 0:
        return None, None, None

    if lines[start].strip() == "/**":
        first_content = lines[start + 1].strip().lstrip("*").strip() if start + 1 <= end else ""
    else:
        after_open = lines[start].split("/**", 1)[1]
        first_content = after_open.strip().lstrip("*").strip()

    return start, end, first_content


def _collect_file(file_path: Path, lines: list[str]) -> list[TestCase]:
    cases: list[TestCase] = []
    n = len(lines)

    i = 0
    while i < n:
        start_match = _TEST_START_RE.match(lines[i])
        if not start_match:
            i += 1
            continue

        top = i
        sig_lines: list[str] = []
        j = i
        while j < n:
            sig_lines.append(lines[j])
            if "{" in lines[j]:
                break
            j += 1
        else:
            i = top + 1
            continue

        sig_text = "\n".join(sig_lines)
        macro_match = _MACRO_RE.search(sig_text)
        if not macro_match:
            i = top + 1
            continue
        macro, suite_name, test_name = macro_match.group(1), macro_match.group(2), macro_match.group(3)

        indent = re.match(r"[ \t]*", lines[top]).group(0)
        del_start, del_end, existing_title = _existing_doc_comment_range(lines, top)
        skip_custom = existing_title is not None and not _GENERATED_TITLE_RE.match(existing_title)

        body_text = _extract_body(lines, j)
        classification, ambiguous = _classify(file_path, body_text)
        context_label = _context_label(file_path, suite_name)
        params = _params_line(macro)

        qualname = f"{suite_name}.{test_name}"
        rendered = _render_doc_comment(indent, classification, context_label, test_name, params)

        cases.append(
            TestCase(
                file_path=file_path,
                qualname=qualname,
                test_name=test_name,
                classification=classification,
                ambiguous=ambiguous,
                insert_line=top,
                delete_start=del_start,
                delete_end=del_end,
                skip_custom_docstring=skip_custom,
                indent=indent,
                rendered=rendered,
            )
        )
        i = j + 1

    return cases


def _apply_edits(lines: list[str], cases: list[TestCase], force: bool) -> tuple[list[str], list[TestCase]]:
    out = list(lines)
    applied: list[TestCase] = []
    for case in sorted(cases, key=lambda c: c.insert_line, reverse=True):
        if case.skip_custom_docstring and not force:
            continue
        rendered_lines = case.rendered.split("\n")
        if case.delete_start is not None and case.delete_end is not None:
            out[case.delete_start : case.delete_end + 1] = rendered_lines
        else:
            out[case.insert_line : case.insert_line] = rendered_lines
        applied.append(case)
    return out, applied


def _excluded(candidate: Path) -> bool:
    parts = candidate.parts
    if EXCLUDED_DIRS & set(parts):
        return True
    return any(part.startswith(EXCLUDED_DIR_PREFIXES) for part in parts)


def _discover_files(target: Path) -> list[Path]:
    if target.is_file():
        return [target] if target.name.endswith((".cpp", ".cc", ".cxx")) else []
    files: list[Path] = []
    for pattern in (
        "*_test.cpp",
        "*_test.cc",
        "*_test.cxx",
        "*Test.cpp",
        "*Tests.cpp",
        "*IT.cpp",
        "*IT.cc",
    ):
        for candidate in target.rglob(pattern):
            if _excluded(candidate):
                continue
            files.append(candidate)
    return sorted(set(files))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("path", nargs="?", default=DEFAULT_TARGET, help="Test file or directory (default: whole repo)")
    parser.add_argument("--check", action="store_true", help="Report only; exit 1 if changes are pending")
    parser.add_argument("--force", action="store_true", help="Overwrite hand-written doc comments too")
    args = parser.parse_args()

    target = (REPO_ROOT / args.path).resolve() if not Path(args.path).is_absolute() else Path(args.path)
    files = _discover_files(target)

    counts = dict.fromkeys(CLASSIFICATIONS, 0)
    documented = 0
    skipped_custom: list[str] = []
    ambiguous: list[str] = []
    changed_files = 0

    for file_path in files:
        source = file_path.read_text(encoding="utf-8")
        lines = source.split("\n")

        cases = _collect_file(file_path, lines)
        if not cases:
            continue

        for case in cases:
            counts[case.classification] += 1
            rel = f"{_display_path(file_path)}::{case.qualname}"
            if case.ambiguous:
                ambiguous.append(rel)
            if case.skip_custom_docstring:
                skipped_custom.append(rel)

        new_lines, applied = _apply_edits(lines, cases, args.force)
        if not applied:
            continue

        changed_files += 1
        documented += len(applied)
        if not args.check:
            file_path.write_text("\n".join(new_lines), encoding="utf-8")

    print(f"Scanned {len(files)} file(s), {sum(counts.values())} test case(s).")
    print("  " + "  ".join(f"[{c}] {n}" for c, n in counts.items()))
    verb = "would document" if args.check else "documented"
    print(f"{verb.capitalize()} {documented} test(s) across {changed_files} file(s).")
    if skipped_custom:
        print(f"Skipped (custom doc comment present, use --force to overwrite): {len(skipped_custom)}")
        for rel in skipped_custom:
            print(f"  - {rel}")
    if ambiguous:
        print(f"Ambiguous classification (needs review, no directory convention matched): {len(ambiguous)}")
        for rel in ambiguous:
            print(f"  - {rel}")

    if args.check and documented:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
