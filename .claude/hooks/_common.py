"""Shared helpers for libbios Claude Code hooks.

All hooks are written in Python (not bash) because the canonical host is
Windows 10/11 and Python 3.12 is a hard project dependency - this guarantees the
hooks run identically on Windows, Linux, and macOS without a POSIX shell.

Hook protocol (Claude Code):
  * Hook input arrives as a single JSON object on stdin.
  * Exit code 0  -> allow / success.
  * Exit code 2  -> block the tool call (stderr is shown to Claude).
  * Any other exit code -> non-blocking error (stderr surfaced to the user).
"""

from __future__ import annotations

import json
import re
import sys
from collections.abc import Iterator
from datetime import UTC, datetime
from pathlib import Path
from typing import Any, Optional

# Repo root is two levels up from .claude/hooks/.
REPO_ROOT = Path(__file__).resolve().parents[2]
LOG_DIR = REPO_ROOT / ".claude" / "logs"

# ---------------------------------------------------------------------------
# Shared doc-corpus helpers - used by doc_link_check.py. scripts/doc_registry.py
# and scripts/linkify_doc_mentions.py are separate, heavier on-demand tools
# with their own independent implementations - they do not import this module.
# ---------------------------------------------------------------------------

_LINK_RE = re.compile(r"\[([^\]]*)\]\(([^)]+)\)")
_HEADING_RE = re.compile(r"^(#{1,6})\s+(.*)$")
_HTML_ANCHOR_RE = re.compile(r'<a\s+[^>]*?(?:id|name)=["\']([^"\']+)["\']', re.I)


def slugify(text: str) -> str:
    """GitHub-flavoured heading slug: lowercase, strip non-word/space/hyphen
    chars, then turn every whitespace char into a hyphen. No run-collapsing,
    no trailing-hyphen trim - matches GitHub's actual behaviour exactly."""
    text = text.strip().lower()
    text = re.sub(r"[^\w\s-]", "", text)
    text = re.sub(r"\s", "-", text)
    return text


def heading_slugs(md_path: Path) -> set[str]:
    """Every anchor target in a Markdown file: heading slugs (with GitHub's
    -1/-2/... suffix applied to duplicates in document order) plus explicit
    ``<a id="...">`` / ``<a name="...">`` anchors, which a renderer honours
    too. Kept in step with ``heading_anchors`` in
    ``scripts/check_doc_links.py`` - see this module's tier note above."""
    slugs: set[str] = set()
    try:
        lines = md_path.read_text(encoding="utf-8").splitlines()
    except OSError:
        return slugs
    seen: dict[str, int] = {}
    in_code_block = False
    for line in lines:
        if line.strip().startswith("```"):
            in_code_block = not in_code_block
            continue
        if in_code_block:
            continue
        slugs.update(_HTML_ANCHOR_RE.findall(line))
        m = _HEADING_RE.match(line)
        if not m:
            continue
        text = m.group(2)
        text = re.sub(r"[`*_]", "", text)
        text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)  # [text](url) -> text
        base = slugify(text)
        n = seen.get(base, 0)
        seen[base] = n + 1
        slugs.add(base if n == 0 else f"{base}-{n}")
    return slugs


def find_broken_links(md_path: Path) -> list[str]:
    """Relative links whose target file doesn't exist, or whose #anchor
    doesn't match any heading slug in the target. External (http/https/
    mailto) links are skipped - this only checks in-repo cross-references."""
    problems: list[str] = []
    try:
        lines = md_path.read_text(encoding="utf-8").splitlines()
    except OSError:
        return problems
    in_code_block = False
    for lineno, line in enumerate(lines, start=1):
        if line.strip().startswith("```"):
            in_code_block = not in_code_block
            continue
        if in_code_block:
            continue
        for m in _LINK_RE.finditer(line):
            raw = m.group(2).strip()
            if not raw:
                continue
            raw_target = raw.split()[0]  # drop a trailing "title" if present
            if raw_target.startswith(("http://", "https://", "mailto:")):
                continue
            path_part, _, anchor = raw_target.partition("#")
            if path_part and not re.search(r"\w", path_part):
                continue  # e.g. "[000N](...)" illustrative prose, not a real link
            # Skip glob/template paths (e.g. /docs/adr/*.md or {NN}-{slug}.md)
            if path_part and ("*" in path_part or "?" in path_part or "{" in path_part):
                continue
            if path_part:
                if path_part.startswith("/"):
                    target_path = (REPO_ROOT / path_part.lstrip("/")).resolve()
                else:
                    target_path = (md_path.parent / path_part).resolve()
                if not target_path.exists():
                    problems.append(f"{md_path}:{lineno}: broken link -> {raw_target} (file not found)")
                    continue
            else:
                target_path = md_path
            if anchor and target_path.suffix.lower() == ".md" and anchor not in heading_slugs(target_path):
                problems.append(
                    f"{md_path}:{lineno}: broken anchor -> {raw_target} "
                    f"(no heading slug '{anchor}' in {target_path.name})"
                )
    return problems


def iter_markdown_files(paths: Optional[list[str]] = None) -> Iterator[Path]:
    """Markdown files to scan: the given paths if any, else every .md file
    under docs/ and .claude/."""
    if paths:
        for raw in paths:
            p = Path(raw)
            if not p.is_absolute():
                p = REPO_ROOT / p
            if p.is_file():
                yield p
        return
    for root_name in ("docs", ".claude"):
        root = REPO_ROOT / root_name
        if root.is_dir():
            yield from sorted(root.rglob("*.md"))


def read_event() -> dict[str, Any]:
    """Parse the hook event JSON from stdin, tolerating an empty stream."""
    raw = sys.stdin.read()
    if not raw.strip():
        return {}
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        return {}


def tool_input(event: dict[str, Any]) -> dict[str, Any]:
    """Return the tool_input mapping from a PreToolUse/PostToolUse event."""
    value = event.get("tool_input", {})
    return value if isinstance(value, dict) else {}


def edited_path(event: dict[str, Any]) -> Optional[Path]:
    """Resolve the file path targeted by a Write/Edit/MultiEdit tool call."""
    fields = tool_input(event)
    raw = fields.get("file_path") or fields.get("path") or fields.get("notebook_path")
    if not raw:
        return None
    p = Path(raw)
    return p if p.is_absolute() else (REPO_ROOT / p)


def now_iso() -> str:
    return datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%SZ")


def append_log(filename: str, line: str):
    """Append a single timestamped line to a file under .claude/logs/."""
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    with (LOG_DIR / filename).open("a", encoding="utf-8") as fh:
        fh.write(f"{now_iso()} {line}\n")


def block(message: str):
    """Emit a blocking decision: stderr + exit 2."""
    sys.stderr.write(message.rstrip() + "\n")
    sys.exit(2)


def allow():
    sys.exit(0)
