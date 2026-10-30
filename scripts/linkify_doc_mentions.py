"""Convert bare *.md file mentions in documentation to Markdown links.

Scans every ``*.md`` file under ``docs/`` and ``.claude/`` for unlinked
references to other documentation files, then replaces them with
``[mention](/abs/path.md)`` links.  Cases that cannot be resolved
deterministically are written to a report for human review.

Resolution strategy (in order):

1. **Exact path** — mention starts with ``docs/``, ``/.claude/``, etc.:
   look up the normalised path directly in the file registry.
2. **Sibling match** — bare filename only, and a file with that name exists
   in the *same directory* as the source file: use that file (single
   candidate wins even if the basename also appears elsewhere).
3. **Global basename** — look up the filename in the whole-corpus index.
   * Exactly one match → resolve.
   * Multiple matches → ambiguous, write to report.
   * No match → not found, write to report.

What is **not** touched:

* Mentions already inside a Markdown link ``[text](target)``.
* Mentions inside backtick code spans.
* Mentions inside fenced code blocks.
* Mentions that contain template placeholders (``{`` or ``}``).

Usage::

    python scripts/linkify_doc_mentions.py               # whole corpus, in-place
    python scripts/linkify_doc_mentions.py --dry-run     # show changes, no writes
    python scripts/linkify_doc_mentions.py docs/foo.md   # single file
    python scripts/linkify_doc_mentions.py --report <p>  # report path override

Exit status: ``0`` when every mention was resolved, ``1`` when any mention
was ambiguous or not found (see ``--report``).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent

SCAN_ROOTS = ["docs", ".claude"]

EXCLUDED_DIRS = {
    ".git",
    ".venv",
    "venv",
    "node_modules",
    ".htmlcov",
    "__pycache__",
    ".idea",
    ".pytest_cache",
    ".ruff_cache",
    "dist",
    "build",
    "state",
}

DEFAULT_REPORT = REPO_ROOT / ".claude" / "state" / "linkify-report.md"

# ---------------------------------------------------------------------------
# Regex helpers
# ---------------------------------------------------------------------------

# Inline code span: one or more backticks.
_CODE_SPAN_RE = re.compile(r"`+[^`]*`+")

# Markdown link or image: [text](target) — also handles [text][ref].
_LINK_RE = re.compile(r"!?\[[^\]]*\](?:\([^)]*\)|\[[^\]]*\])")

# Opening/closing code fence.
_FENCE_RE = re.compile(r"^ {0,3}(`{3,}|~{3,})")

# ATX heading: up to 3 leading spaces, then 1–6 '#', then a space.
# Lines matching this are skipped — headings are topic labels, not file refs.
_HEADING_RE = re.compile(r"^ {0,3}#{1,6} ")

# A bare .md file mention:
#   - optional leading /
#   - optional path prefix (word chars, hyphens, dots, slashes)
#   - filename stem (must start AND end with a word char — prevents "non-.md")
#   - literal ".md"
#
# Negative lookbehind: not after ( [ / or a word char (left word boundary —
#   prevents "lan.md" matching inside "plan.md").
# Negative lookahead: not before ) ] # or a word char (right word boundary).
_MD_MENTION_RE = re.compile(
    r"(?<![(\[/\w])"  # left boundary: not after ( [ / or word char
    r"(/?\.?"  # optional leading / or ./
    r"(?:[\w][\w.\-]*/)*"  # zero or more path components  (dir/dir/)
    r"[\w][\w.\-]*\w"  # stem: starts AND ends with word char (no trailing -)
    r"\.md)"  # extension
    r"(?![)\]#\w])"  # right boundary: not before ) ] # or word char
)


# ---------------------------------------------------------------------------
# Registry
# ---------------------------------------------------------------------------


def build_registry() -> list[str]:
    """Return sorted repo-root-relative paths of all *.md files in the scan roots.

    :return: sorted list of paths like ``"docs/README.md"``.
    """
    found: set[str] = set()
    for root_name in SCAN_ROOTS:
        root = REPO_ROOT / root_name
        if not root.is_dir():
            continue
        for md in root.rglob("*.md"):
            if any(part in EXCLUDED_DIRS for part in md.parts):
                continue
            found.add(str(md.relative_to(REPO_ROOT)))
    return sorted(found)


def build_indices(
    registry: list[str],
) -> tuple[dict[str, str], dict[str, list[str]]]:
    """Build lookup structures from the registry.

    :param registry: sorted list of repo-root-relative paths.
    :return: ``(by_path, by_name)`` where *by_path* maps a normalised path
             string to itself (for O(1) existence checks) and *by_name* maps
             each basename to the list of registry paths that share it.
    """
    by_path: dict[str, str] = {}
    by_name: dict[str, list[str]] = {}

    for path in registry:
        # Index by path, both with and without a leading slash.
        by_path[path] = path
        by_path["/" + path] = path
        by_name.setdefault(Path(path).name, []).append(path)

    return by_path, by_name


# ---------------------------------------------------------------------------
# Protected-region computation
# ---------------------------------------------------------------------------


def _protected_spans(line: str) -> list[tuple[int, int]]:
    """Return sorted (start, end) spans on *line* that must not be modified.

    Covers existing Markdown links and inline code spans.  Spans are
    *non-overlapping* (later regex matches inside an earlier one are redundant
    but do not break anything).

    :param line: a single line of Markdown source (no trailing newline).
    :return: list of ``(start, end)`` exclusive ranges.
    """
    spans: list[tuple[int, int]] = []
    for pattern in (_LINK_RE, _CODE_SPAN_RE):
        for m in pattern.finditer(line):
            spans.append((m.start(), m.end()))
    spans.sort()
    return spans


def _is_protected(pos: int, end: int, spans: list[tuple[int, int]]) -> bool:
    """True if the interval [pos, end) overlaps any protected span.

    :param pos: start of the candidate interval.
    :param end: exclusive end of the candidate interval.
    :param spans: sorted list from :func:`_protected_spans`.
    :return: whether the interval overlaps any protected span.
    """
    for s, e in spans:
        if s >= end:
            break
        if pos < e and end > s:
            return True
    return False


# ---------------------------------------------------------------------------
# Mention resolver
# ---------------------------------------------------------------------------


def resolve_mention(
    mention: str,
    source_rel: str,
    by_path: dict[str, str],
    by_name: dict[str, list[str]],
) -> tuple[Optional[str], str]:
    """Resolve a bare mention to an absolute registry path, or explain failure.

    :param mention: the text as found in the source (e.g. ``"plan.md"`` or
                    ``"docs/dev/foo.md"``).
    :param source_rel: repo-root-relative path of the file containing the
                       mention (used for sibling lookups).
    :param by_path: path → path index from :func:`build_indices`.
    :param by_name: basename → [path, ...] index from :func:`build_indices`.
    :return: ``(resolved_path, reason)`` where *resolved_path* is the
             repo-root-relative path with a leading ``/`` prefix (or ``None``
             when resolution fails) and *reason* describes what happened.
    """
    # Skip template paths.
    if "{" in mention or "}" in mention:
        return None, "template placeholder — skipped"

    norm = mention.lstrip("/")  # strip any leading slash for lookup

    # 1. Exact path match.
    if norm in by_path:
        resolved = "/" + by_path[norm]
        # Don't convert a file's own name into a self-link (e.g. the title
        # "# CLAUDE.md - …" in CLAUDE.md itself).
        if by_path[norm] == source_rel:
            return None, "self-reference — skipped"
        return resolved, "exact path match"

    # Does the mention look like it has a path prefix?
    has_path = "/" in norm

    if has_path:
        # It claimed to be a path but wasn't in the registry.
        return None, f"path not in registry: {norm}"

    # 2. Bare filename only.
    name = Path(norm).name  # == norm when no slash

    # 2a. Sibling match — prefer files in the same directory.
    source_dir = str(Path(source_rel).parent)
    sibling = f"{source_dir}/{name}" if source_dir != "." else name
    if sibling in by_path:
        if by_path[sibling] == source_rel:
            return None, "self-reference — skipped"
        return "/" + by_path[sibling], "sibling file in same directory"

    # 2b. Global basename lookup.
    candidates = by_name.get(name, [])
    if len(candidates) == 1:
        if candidates[0] == source_rel:
            return None, "self-reference — skipped"
        return "/" + candidates[0], "unique basename in corpus"
    if len(candidates) > 1:
        all_paths = ", ".join(candidates)
        return None, f"ambiguous — {len(candidates)} files named {name!r}: {all_paths}"

    return None, f"not found in registry: {name!r}"


# ---------------------------------------------------------------------------
# Line transformer
# ---------------------------------------------------------------------------


def transform_line(
    line: str,
    source_rel: str,
    by_path: dict[str, str],
    by_name: dict[str, list[str]],
) -> tuple[str, list[dict[str, object]]]:
    """Replace bare .md mentions in *line* with Markdown links.

    Returns the (possibly modified) line and a list of unresolvable mention
    records for the report.

    :param line: original source line.
    :param source_rel: repo-root-relative path of the source file.
    :param by_path: from :func:`build_indices`.
    :param by_name: from :func:`build_indices`.
    :return: ``(new_line, unresolvable_list)`` where each unresolvable entry
             is ``{source, line_text, mention, reason}``.
    """
    # ATX headings are topic labels, not file references — skip entirely.
    if _HEADING_RE.match(line):
        return line, []

    protected = _protected_spans(line)
    unresolvable: list[dict[str, object]] = []

    # Collect replacements first (right-to-left to preserve positions).
    replacements: list[tuple[int, int, str]] = []

    for m in _MD_MENTION_RE.finditer(line):
        mention = m.group(1)
        start, end = m.start(1), m.end(1)

        if _is_protected(start, end, protected):
            continue

        resolved, reason = resolve_mention(mention, source_rel, by_path, by_name)
        if resolved is None:
            if "skipped" not in reason:
                unresolvable.append(
                    {
                        "mention": mention,
                        "reason": reason,
                    }
                )
            continue

        # Build the replacement link.
        link = f"[{mention}]({resolved})"
        replacements.append((start, end, link))

    # Apply replacements right-to-left.
    result = line
    for start, end, link in sorted(replacements, key=lambda r: -r[0]):
        result = result[:start] + link + result[end:]

    return result, unresolvable


# ---------------------------------------------------------------------------
# File processor
# ---------------------------------------------------------------------------


def process_file(
    path: Path,
    by_path: dict[str, str],
    by_name: dict[str, list[str]],
    dry_run: bool = False,
) -> tuple[bool, list[dict[str, object]]]:
    """Process one Markdown file, optionally writing changes back.

    :param path: absolute path to the file.
    :param by_path: from :func:`build_indices`.
    :param by_name: from :func:`build_indices`.
    :param dry_run: when ``True``, compute changes but do not write.
    :return: ``(changed, unresolvable_list)`` where *changed* is ``True``
             when the file was (or would be) modified.
    """
    source_rel = str(path.relative_to(REPO_ROOT))
    original = path.read_text(encoding="utf-8")
    lines = original.splitlines(keepends=True)

    new_lines: list[str] = []
    all_unresolvable: list[dict[str, object]] = []
    in_fence = False
    fence_marker: Optional[str] = None
    changed = False

    for lineno, raw in enumerate(lines, 1):
        stripped = raw.rstrip("\n")
        fence = _FENCE_RE.match(stripped)
        if fence:
            marker = fence.group(1)[0]
            if not in_fence:
                in_fence, fence_marker = True, marker
            elif marker == fence_marker:
                in_fence, fence_marker = False, None
            new_lines.append(raw)
            continue

        if in_fence:
            new_lines.append(raw)
            continue

        new_stripped, unresolvable = transform_line(stripped, source_rel, by_path, by_name)
        for entry in unresolvable:
            entry["file"] = source_rel
            entry["line"] = lineno
        all_unresolvable.extend(unresolvable)

        if new_stripped != stripped:
            changed = True
            eol = raw[len(stripped) :]
            new_lines.append(new_stripped + eol)
        else:
            new_lines.append(raw)

    if changed and not dry_run:
        path.write_text("".join(new_lines), encoding="utf-8")

    return changed, all_unresolvable


# ---------------------------------------------------------------------------
# Report writer
# ---------------------------------------------------------------------------


def write_report(
    unresolvable: list[dict[str, object]],
    report_path: Path,
) -> None:
    """Write an actionable review report for unresolvable mentions.

    :param unresolvable: list of unresolvable entry dicts from
                         :func:`process_file`.
    :param report_path: path to write the report to.
    """
    report_path.parent.mkdir(parents=True, exist_ok=True)
    lines: list[str] = [
        "# Linkify Report — Unresolvable .md Mentions\n",
        "\n",
        "Generated by `scripts/linkify_doc_mentions.py`.  "
        "Each row is a bare `.md` mention that could **not** be automatically\n",
        "converted to a link.  Resolve each item manually and re-run the script.\n",
        "\n",
        "## How to resolve\n",
        "\n",
        "- **Ambiguous basename** (`README.md`, `plan.md`, etc.): replace the bare\n",
        "  mention with a full link to the correct file manually.\n",
        "- **Path not in registry**: the path looks like a real path but the file\n",
        "  does not exist.  Either create the file or fix the mention.\n",
        "- **Not found**: the filename does not match any file in the corpus.\n",
        "  Check for typos or create the missing file.\n",
        "\n",
        "## Unresolvable mentions\n",
        "\n",
        "| File | Line | Mention | Reason |\n",
        "|------|------|---------|--------|\n",
    ]

    for entry in sorted(unresolvable, key=lambda e: (e["file"], e["line"])):
        f = entry["file"]
        ln = entry["line"]
        mention = entry["mention"]
        reason = entry["reason"]
        lines.append(f"| `{f}` | {ln} | `{mention}` | {reason} |\n")

    lines.append("\n")
    lines.append(f"Total: {len(unresolvable)} unresolvable mention(s).\n")

    report_path.write_text("".join(lines), encoding="utf-8")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def iter_targets(paths: list[Path]) -> list[Path]:
    """Expand paths to a sorted deduplicated list of ``.md`` files.

    :param paths: files or directories; directories are recursively searched.
    :return: sorted list of absolute ``.md`` file paths.
    """
    found: set[Path] = set()
    for p in paths:
        if p.is_file() and p.suffix == ".md":
            found.add(p.resolve())
        elif p.is_dir():
            for md in p.rglob("*.md"):
                if any(part in EXCLUDED_DIRS for part in md.parts):
                    continue
                found.add(md.resolve())
    return sorted(found)


def main(argv: Optional[list[str]] = None) -> int:
    """Entry point.

    :return: exit code (0 = all resolved, 1 = unresolvable items remain).
    """
    parser = argparse.ArgumentParser(description="Convert bare .md mentions in docs to Markdown links.")
    parser.add_argument(
        "paths",
        nargs="*",
        help="Files or directories to process (default: docs/ and .claude/).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print changes without modifying any files.",
    )
    parser.add_argument(
        "--report",
        default=str(DEFAULT_REPORT),
        metavar="PATH",
        help=f"Path for the unresolvable-mentions report (default: {DEFAULT_REPORT}).",
    )
    args = parser.parse_args(argv)

    # Determine targets.
    raw_targets = [Path(p) for p in args.paths] if args.paths else [REPO_ROOT / r for r in SCAN_ROOTS]

    targets = iter_targets(raw_targets)
    if not targets:
        print("linkify: no .md files found in the given paths.", file=sys.stderr)
        return 0

    # Build registry (exclude the files being processed? No — we want
    # cross-file links, and a file can reference itself only if it mentions its
    # own path, which is unlikely to produce a useful link anyway).
    registry = []
    for root_name in SCAN_ROOTS:
        root = REPO_ROOT / root_name
        if not root.is_dir():
            continue
        for md in root.rglob("*.md"):
            if any(part in md.parts for part in EXCLUDED_DIRS):
                continue
            registry.append(str(md.relative_to(REPO_ROOT)))
    registry.sort()

    by_path, by_name = build_indices(registry)

    all_unresolvable: list[dict[str, object]] = []
    changed_count = 0

    for file_path in targets:
        changed, unresolvable = process_file(file_path, by_path, by_name, dry_run=args.dry_run)
        if changed:
            changed_count += 1
            rel = file_path.relative_to(REPO_ROOT)
            verb = "would update" if args.dry_run else "updated"
            print(f"  {verb}: {rel}")
        all_unresolvable.extend(unresolvable)

    # Summary.
    mode = " (dry-run)" if args.dry_run else ""
    print(
        f"\n{len(targets)} file(s) scanned{mode}, "
        f"{changed_count} modified, "
        f"{len(all_unresolvable)} unresolvable mention(s)."
    )

    if all_unresolvable:
        report_path = Path(args.report)
        write_report(all_unresolvable, report_path)
        print(f"Report written to: {report_path.relative_to(REPO_ROOT)}")

    return 1 if all_unresolvable else 0


if __name__ == "__main__":
    raise SystemExit(main())
