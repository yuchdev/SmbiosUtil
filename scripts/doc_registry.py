"""Build a registry of all *.md files under docs/ and .claude/, detect
broken cross-references to *.md files, and suggest rename candidates.

This is the backend for ``/doc-registry`` and the scan-mode of the
``update-docs`` loop.  It does **not** validate anchors — use
``check_doc_links.py`` for that; its job is to map the corpus and detect
missing-file references only.

Usage::

    python scripts/doc_registry.py            # human-readable report (default)
    python scripts/doc_registry.py --json     # JSON output to stdout
    python scripts/doc_registry.py --cursor <path>
                                              # write JSON to <path> (cursor file)

Exit status: ``0`` when no missing .md references exist, ``1`` otherwise.
"""

from __future__ import annotations

import argparse
import difflib
import json
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
    "state",  # .claude/state/ holds cursor files, not documentation
}

_LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
_CODE_SPAN_RE = re.compile(r"`+[^`]*`+")
_FENCE_RE = re.compile(r"^ {0,3}(`{3,}|~{3,})")
_SCHEME_RE = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")


# ---------------------------------------------------------------------------
# Registry builder
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
            rel = md.relative_to(REPO_ROOT)
            found.add(str(rel))
    return sorted(found)


# ---------------------------------------------------------------------------
# Link extractor
# ---------------------------------------------------------------------------


def _is_skippable_target(target: str) -> bool:
    """True for non-file targets (URLs, anchors, templates, globs)."""
    if not target:
        return True
    if target.startswith("#"):
        return True  # in-page anchor only — no file to check
    if "{" in target or "}" in target:
        return True  # template path like {NN}-{slug}.md
    if "*" in target or "?" in target:
        return True  # glob
    return bool(_SCHEME_RE.match(target))  # http:, https:, mailto:, …


def _resolve_target(target_path: str, source: Path) -> Path:
    """Resolve a link's file part to an absolute path.

    ``/x`` is repo-root-relative; anything else is relative to the directory
    of *source*.
    """
    if target_path.startswith("/"):
        return (REPO_ROOT / target_path.lstrip("/")).resolve()
    return (source.parent / target_path).resolve()


def extract_md_links(
    source_path: Path,
) -> list[dict[str, object]]:
    """Extract all links from *source_path* that point at *.md files.

    Returns a list of dicts::

        {
            "source": "docs/foo.md",      # repo-root-relative
            "line": 42,
            "target": "bar/baz.md",       # raw text from the Markdown link
            "resolved": "docs/bar/baz.md" # repo-root-relative resolved path
        }

    Links that are skippable (URLs, in-page anchors, templates) are omitted.
    Links to non-``.md`` files are also omitted; those are ``check_doc_links``
    territory.

    :param source_path: absolute path to the Markdown source file.
    :return: list of link descriptors.
    """
    results: list[dict[str, object]] = []
    in_fence = False
    fence_marker: Optional[str] = None
    source_rel = str(source_path.relative_to(REPO_ROOT))

    for lineno, raw in enumerate(source_path.read_text(encoding="utf-8").splitlines(), 1):
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

        line = _CODE_SPAN_RE.sub("", raw)
        for raw_target in _LINK_RE.findall(line):
            target = raw_target.split()[0]  # strip optional "title"
            # Strip any fragment from the file part
            file_part, _, _ = target.partition("#")
            if _is_skippable_target(file_part):
                continue
            if not file_part.lower().endswith(".md"):
                continue  # non-.md files are out of scope for this tool

            resolved_abs = _resolve_target(file_part, source_path)
            try:
                resolved_rel = str(resolved_abs.relative_to(REPO_ROOT))
            except ValueError:
                resolved_rel = str(resolved_abs)  # outside repo — very unusual

            results.append(
                {
                    "source": source_rel,
                    "line": lineno,
                    "target": file_part,
                    "resolved": resolved_rel,
                }
            )

    return results


# ---------------------------------------------------------------------------
# Rename-candidate finder
# ---------------------------------------------------------------------------


def _path_similarity(a: str, b: str) -> float:
    """Return a [0, 1] similarity score between two repo-relative paths."""
    return difflib.SequenceMatcher(None, a, b).ratio()


def find_rename_candidates(
    missing_resolved: str,
    registry: list[str],
) -> list[dict[str, object]]:
    """Return rename candidates for a missing file from the live registry.

    Confidence levels:

    * ``high``   – exactly one registry entry shares the same basename.
    * ``medium`` – multiple entries share the basename, OR the stem similarity
                   (``difflib``) is ≥ 0.75 but basename differs.
    * None       – no plausible match found.

    :param missing_resolved: repo-root-relative path that does not exist.
    :param registry: list of repo-root-relative paths that do exist.
    :return: list of candidate dicts ``{path, confidence, reason}``.
    """
    missing_path = Path(missing_resolved)
    missing_name = missing_path.name  # e.g. "README.md"
    missing_stem = missing_path.stem  # e.g. "README"

    basename_matches = [r for r in registry if Path(r).name == missing_name]
    if len(basename_matches) == 1:
        return [
            {
                "path": basename_matches[0],
                "confidence": "high",
                "reason": "exact basename match",
            }
        ]
    if len(basename_matches) > 1:
        # Narrow by directory similarity
        scored = sorted(
            basename_matches,
            key=lambda r: _path_similarity(missing_resolved, r),
            reverse=True,
        )
        return [
            {
                "path": p,
                "confidence": "medium",
                "reason": f"basename match ({len(basename_matches)} candidates)",
            }
            for p in scored
        ]

    # No basename match — try fuzzy stem comparison
    similar = [r for r in registry if _path_similarity(missing_stem, Path(r).stem) >= 0.75]
    if similar:
        return [
            {
                "path": p,
                "confidence": "medium",
                "reason": f"stem similarity ≥ 0.75 (ratio {_path_similarity(missing_stem, Path(p).stem):.2f})",
            }
            for p in sorted(similar, key=lambda r: -_path_similarity(missing_stem, Path(r).stem))
        ]

    return []


# ---------------------------------------------------------------------------
# Report builder
# ---------------------------------------------------------------------------


def build_report(
    registry: list[str],
    all_links: list[dict[str, object]],
) -> dict[str, object]:
    """Cross-reference *all_links* against *registry* and classify each missing entry.

    Returns a report dict::

        {
            "registry": [...],
            "missing": [
                {
                    "source": "...",
                    "line": N,
                    "target": "...",
                    "resolved": "...",
                    "candidates": [{path, confidence, reason}, ...]
                }, ...
            ],
            "summary": {
                "registry_size": N,
                "total_md_links": N,
                "missing_count": N,
                "high_confidence": N,   # likely renames
                "needs_review": N,      # ambiguous or no candidates
            }
        }

    :param registry: list of existing repo-root-relative ``.md`` paths.
    :param all_links: output of :func:`extract_md_links` across the corpus.
    :return: report dict.
    """
    registry_set = set(registry)
    seen_missing: set[str] = set()  # deduplicate by (source, resolved)
    missing: list[dict[str, object]] = []

    for link in all_links:
        resolved = str(link["resolved"])
        if resolved in registry_set:
            continue  # link is fine

        dedup_key = f"{link['source']}::{resolved}"
        if dedup_key in seen_missing:
            continue
        seen_missing.add(dedup_key)

        candidates = find_rename_candidates(resolved, registry)
        missing.append(
            {
                "source": link["source"],
                "line": link["line"],
                "target": link["target"],
                "resolved": resolved,
                "candidates": candidates,
            }
        )

    high = sum(1 for m in missing if any(c["confidence"] == "high" for c in m["candidates"]))
    needs_review = len(missing) - high

    return {
        "registry": registry,
        "missing": missing,
        "summary": {
            "registry_size": len(registry),
            "total_md_links": len(all_links),
            "missing_count": len(missing),
            "high_confidence": high,
            "needs_review": needs_review,
        },
    }


# ---------------------------------------------------------------------------
# Human-readable formatter
# ---------------------------------------------------------------------------


def format_report(report: dict[str, object]) -> str:
    """Render *report* as a human-readable string.

    :param report: dict from :func:`build_report`.
    :return: formatted multi-line string.
    """
    lines: list[str] = []
    summary = report["summary"]
    missing = report["missing"]

    lines.append("=== libbios Documentation Registry ===\n")
    lines.append(f"Registry: {summary['registry_size']} Markdown files found in " + ", ".join(SCAN_ROOTS))
    lines.append(f"Total .md cross-references scanned: {summary['total_md_links']}\n")

    if not missing:
        lines.append("All .md cross-references resolve correctly. No missing files.\n")
        return "\n".join(lines)

    lines.append(f"=== Missing .md References ({summary['missing_count']} total) ===\n")

    for item in missing:
        candidates = item["candidates"]
        top = candidates[0] if candidates else None
        if top and top["confidence"] == "high":
            tag = "[HIGH] Likely rename"
        elif top:
            tag = "[MEDIUM] Possible rename"
        else:
            tag = "[REVIEW] No candidates found"

        lines.append(f"{tag}")
        lines.append(f"  Source : {item['source']}:{item['line']}")
        lines.append(f"  Missing: {item['resolved']}")
        if candidates:
            for c in candidates:
                lines.append(f"  Candidate ({c['confidence']}): {c['path']}  [{c['reason']}]")
        lines.append("")

    lines.append("=== Summary ===")
    lines.append(f"  Registry size       : {summary['registry_size']} files")
    lines.append(f"  Missing .md refs    : {summary['missing_count']}")
    lines.append(f"  High-confidence     : {summary['high_confidence']}  (likely renames)")
    lines.append(f"  Needs human review  : {summary['needs_review']}")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def main(argv: Optional[list[str]] = None) -> int:
    """Entry point.

    :return: exit code (0 = clean, 1 = missing references found).
    """
    parser = argparse.ArgumentParser(description="Build a .md file registry and detect broken cross-references.")
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit JSON to stdout instead of the human-readable report.",
    )
    parser.add_argument(
        "--cursor",
        metavar="PATH",
        help=(
            "Write JSON report to PATH (cursor file for the update-docs loop). "
            "Implies --json for the file; human-readable report still goes to stdout "
            "unless --json is also given."
        ),
    )
    args = parser.parse_args(argv)

    registry = build_registry()

    all_links: list[dict[str, object]] = []
    for rel_path in registry:
        abs_path = REPO_ROOT / rel_path
        try:
            all_links.extend(extract_md_links(abs_path))
        except (OSError, UnicodeDecodeError) as exc:
            print(f"Warning: could not read {rel_path}: {exc}", file=sys.stderr)

    report = build_report(registry, all_links)

    if args.cursor:
        cursor_path = Path(args.cursor)
        cursor_path.parent.mkdir(parents=True, exist_ok=True)
        cursor_path.write_text(json.dumps(report, indent=2), encoding="utf-8")

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(format_report(report))

    return 1 if report["summary"]["missing_count"] > 0 else 0


if __name__ == "__main__":
    raise SystemExit(main())
