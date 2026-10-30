#!/usr/bin/env python3
"""PostToolUse (Write|Edit|MultiEdit) + Stop: documentation reference integrity.

Checks that Markdown relative links resolve and that `#anchor` fragments match
a real heading slug in the target file - entirely inline, no dependency on
`scripts/` (the link/anchor logic lives in `_common.py`). This is the fast
auto-gate; `scripts/check_doc_links.py` is a separate, more thorough on-demand
checker the `/link-check` skill runs - the two are complementary, not
duplicates of each other.

* **PostToolUse** - when the edited file is Markdown, check that file's outbound
  links/anchors immediately (scoped, cheap).
* **Stop** - if any Markdown changed in the working tree this session, run the
  full `docs/` + `.claude/` scan, which also catches breakage caused by a file
  other than the one just edited (e.g. a renamed heading whose old anchor is
  still linked from elsewhere).

Also runnable standalone: ``python doc_link_check.py --check [<path> ...]`` -
a quick ad-hoc check independent of the `/link-check` skill (which uses the
heavier `scripts/check_doc_links.py` instead). With no paths, scans the whole
corpus.

Always **non-blocking** (exit 0 from the hook): it surfaces problems as a
reminder but never wedges a session. The CLI mode exits 1 on findings so it
composes with the `/link-check` skill and CI.
"""

from __future__ import annotations

import subprocess
import sys

from _common import REPO_ROOT, allow, append_log, edited_path, find_broken_links, iter_markdown_files, read_event


def check(paths: list[str] | None) -> list[str]:
    problems: list[str] = []
    for md in iter_markdown_files(paths):
        problems.extend(find_broken_links(md))
    return problems


def _changed_markdown() -> list[str]:
    try:
        proc = subprocess.run(
            ["git", "diff", "--name-only", "HEAD"],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=15,
            check=False,
        )
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return []
    if proc.returncode != 0:
        return []
    return [ln for ln in proc.stdout.splitlines() if ln.strip().lower().endswith(".md")]


def _hook_mode() -> None:
    event = read_event()
    target = edited_path(event)

    if target is not None:
        if target.suffix.lower() != ".md" or not target.exists():
            allow()
        problems = check([str(target)])
    else:
        if not _changed_markdown():
            allow()
        problems = check(None)

    if problems:
        append_log("doc-link-check.log", f"{len(problems)} problem(s) found")
        sys.stderr.write(
            "doc_link_check: dangling documentation references (non-blocking):\n"
            + "\n".join(problems)
            + "\nRun `/link-check` to review.\n"
        )
    allow()


def _cli_mode(argv: list[str]) -> None:
    args = argv[1:] if argv and argv[0] == "--check" else argv
    problems = check(args if args else None)
    for p in problems:
        print(p)
    if problems:
        print(f"\ndoc_link_check: {len(problems)} problem(s) found.")
        sys.exit(1)
    print("doc_link_check: clean.")
    sys.exit(0)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        _cli_mode(sys.argv[1:])
    else:
        _hook_mode()
