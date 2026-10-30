"""Validate Markdown cross-references: link targets and heading anchors resolve.

Scans ``*.md`` files for inline links ``[text](target)`` /
``[text](target#anchor)`` and in-page anchors ``[text](#anchor)`` and checks
that every target file exists and every ``#anchor`` matches a real heading in
the target file (GitHub-style heading slug).

Link-target resolution:

- absolute-from-repo-root ``/docs/...`` resolves against the repo root;
- everything else resolves relative to the linking file's directory.

Deliberately **skipped** (not validated): external URLs (``http(s):``,
``mailto:`` and other schemes), template paths containing ``{`` / ``}`` (e.g.
``{NN}-{task-slug}.md``), links inside fenced code blocks, and any link
syntax sitting inside an inline ``code span``.

Usage::

    python scripts/check_doc_links.py                 # scan the whole repo
    python scripts/check_doc_links.py --check          # same; explicit
    python scripts/check_doc_links.py docs/ a.md       # restrict to given paths

This is the comprehensive, on-demand checker the `/link-check` skill runs - a separate,
faster inline check runs automatically on every edit as the `doc_link_check` PostToolUse/Stop
hook (`.claude/hooks/doc_link_check.py`); the two are complementary, not duplicates - the hook
never imports or shells out to this script.

Exit status is ``1`` when any dangling link or anchor is found, else ``0``.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent

# Directories that hold vendored / generated Markdown we must not scan.
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
}

# Inline Markdown link / image: ``[text](target)`` or ``![alt](target)``. The
# target group is everything up to the closing paren.
_LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
# Inline code span: ``code`` (one or more backticks). Removed before link search
# so link-shaped text inside code is not treated as a real link.
_CODE_SPAN_RE = re.compile(r"`+[^`]*`+")
# Opening/closing code fence: up to 3 leading spaces, then ``` or ~~~.
_FENCE_RE = re.compile(r"^ {0,3}(`{3,}|~{3,})")
# ATX heading: up to 3 leading spaces, 1-6 '#', then a space.
_HEADING_RE = re.compile(r"^ {0,3}(#{1,6})\s+(.*?)\s*#*\s*$")
# External schemes we never resolve as file paths.
_SCHEME_RE = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")


_HTML_ANCHOR_RE = re.compile(r'<a\s+[^>]*?(?:id|name)=["\']([^"\']+)["\']', re.I)


def slugify(heading_text: str) -> str:
    """Return the GitHub-style anchor slug for a heading's text.

    Mirrors github-slugger: strip ends, lowercase, delete every character that
    is not a word char / whitespace / hyphen, then turn each remaining
    whitespace character into a single hyphen (runs are **not** collapsed).

    :param heading_text: the heading text without the leading ``#`` markers.
    :return: the anchor slug a Markdown renderer would assign to the heading.
    """
    text = heading_text.strip().lower()
    text = re.sub(r"[^\w\s-]", "", text)
    text = re.sub(r"\s", "-", text)
    return text


def heading_anchors(path: Path) -> set[str]:
    """Return the set of anchor targets in *path*.

    Two kinds count, because a Markdown renderer honours both:

    * **Heading slugs** - duplicate slugs get the ``-1``, ``-2`` ... suffixes a
      renderer would assign, so a link to the second identical heading still
      validates. Headings inside fenced code blocks are ignored.
    * **Explicit HTML anchors** - ``<a id="foo">`` / ``<a name="foo">``. Docs
      adapted from sources with stable hand-written anchors (e.g. the Google
      Python style guide's ``#s1-lint``) rely on these, and treating them as
      absent produces a wall of false "missing anchor" reports.
    """
    anchors: set[str] = set()
    seen: dict[str, int] = {}
    in_fence = False
    fence_marker: Optional[str] = None

    for line in path.read_text(encoding="utf-8").splitlines():
        fence = _FENCE_RE.match(line)
        if fence:
            marker = fence.group(1)[0]
            if not in_fence:
                in_fence, fence_marker = True, marker
            elif marker == fence_marker:
                in_fence, fence_marker = False, None
            continue
        if in_fence:
            continue
        anchors.update(_HTML_ANCHOR_RE.findall(line))
        heading = _HEADING_RE.match(line)
        if not heading:
            continue
        base = slugify(heading.group(2))
        count = seen.get(base, 0)
        anchors.add(base if count == 0 else f"{base}-{count}")
        seen[base] = count + 1
    return anchors


def _is_skippable(target: str) -> bool:
    """True for targets we never resolve (URLs, template paths, empty)."""
    if not target:
        return True
    if "{" in target or "}" in target:  # template path, e.g. {NN}-{slug}.md
        return True
    if "*" in target or "?" in target:  # glob pattern, e.g. /docs/adr/*.md
        return True
    if target.startswith("#"):
        return False  # in-page anchor - handled by the caller
    return bool(_SCHEME_RE.match(target))  # http:, https:, mailto:, ...


def _resolve(target_path: str, source: Path) -> Path:
    """Resolve a link's file part to an absolute path.

    ``/x`` is repo-root-relative; anything else is relative to the directory of
    the *source* file.
    """
    if target_path.startswith("/"):
        return (REPO_ROOT / target_path.lstrip("/")).resolve()
    return (source.parent / target_path).resolve()


def check_file(path: Path) -> list[str]:
    """Return a list of ``file:line: ...`` problems for one Markdown file."""
    problems: list[str] = []
    in_fence = False
    fence_marker: Optional[str] = None
    anchor_cache: dict[Path, set[str]] = {}

    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        fence = _FENCE_RE.match(raw)
        if fence:
            marker = fence.group(1)[0]
            if not in_fence:
                in_fence, fence_marker = True, marker
            elif marker == fence_marker:
                in_fence, fence_marker = False, None
            continue
        if in_fence:
            continue

        line = _CODE_SPAN_RE.sub("", raw)  # drop inline code spans
        for target in _LINK_RE.findall(line):
            target = target.split()[0]  # strip any "title" after the path
            if _is_skippable(target):
                continue

            file_part, _, anchor = target.partition("#")

            if file_part == "":
                # In-page anchor: must exist in this same file.
                anchors = anchor_cache.setdefault(path, heading_anchors(path))
                if anchor and anchor not in anchors:
                    problems.append(f"{path}:{lineno}: missing anchor '#{anchor}' in this file")
                continue

            resolved = _resolve(file_part, path)
            if not resolved.exists():
                problems.append(
                    f"{path}:{lineno}: dangling link -> {target} "
                    f"(resolved {resolved.relative_to(REPO_ROOT) if REPO_ROOT in resolved.parents else resolved})"
                )
                continue
            if anchor and resolved.is_file() and resolved.suffix == ".md":
                anchors = anchor_cache.setdefault(resolved, heading_anchors(resolved))
                if anchor not in anchors:
                    problems.append(f"{path}:{lineno}: missing anchor '#{anchor}' in {file_part}")
    return problems


def iter_markdown(paths: list[Path]) -> list[Path]:
    """Expand the given paths into a sorted list of ``*.md`` files."""
    found: set[Path] = set()
    for p in paths:
        if p.is_file() and p.suffix == ".md":
            found.add(p)
        elif p.is_dir():
            for md in p.rglob("*.md"):
                if any(part in EXCLUDED_DIRS for part in md.parts):
                    continue
                found.add(md)
    return sorted(found)


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Validate Markdown links and anchors.")
    parser.add_argument(
        "paths",
        nargs="*",
        default=["."],
        help="Files or directories to scan (default: whole repo).",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Report-only flag (accepted for hook compatibility; always report-only).",
    )
    args = parser.parse_args(argv)

    targets = iter_markdown([Path(p) for p in args.paths])
    problems: list[str] = []
    for md in targets:
        problems.extend(check_file(md))

    for problem in problems:
        print(problem)
    print(f"\n{len(targets)} file(s) scanned, {len(problems)} problem(s) found.")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
