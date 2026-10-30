#!/usr/bin/env python3
"""SessionStart hook - seed the session with live repo context.

Emits, as additional context for Claude:
  * current git branch
  * the last 5 commits (one line each)
  * any open P0/P1 issues (via the GitHub CLI if authenticated; silent if not)

SessionStart context is provided by printing to stdout (the harness injects
stdout from a SessionStart hook into the conversation context).
"""

from __future__ import annotations

import subprocess

from _common import REPO_ROOT


def _git(args: list[str]) -> str:
    try:
        out = subprocess.run(
            ["git", *args],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=15,
            check=False,
        )
        return out.stdout.strip()
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return ""


def _open_priority_issues() -> str:
    """Best-effort P0/P1 lookup via gh. Returns empty string when unavailable."""
    try:
        out = subprocess.run(
            [
                "gh", "issue", "list",
                "--state", "open",
                "--label", "P0,P1",
                "--limit", "20",
                "--json", "number,title,labels",
                "--template",
                "{{range .}}  #{{.number}} {{.title}}\n{{end}}",
            ],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=20,
            check=False,
        )
        return out.stdout.strip() if out.returncode == 0 else ""
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return ""


def main() -> None:
    branch = _git(["rev-parse", "--abbrev-ref", "HEAD"]) or "(unknown)"
    log = _git(["log", "-5", "--pretty=format:  %h %s"]) or "  (no commits)"
    issues = _open_priority_issues()

    lines = [
        "# libbios - session context",
        f"Branch: {branch}",
        "",
        "Recent commits:",
        log,
    ]
    if issues:
        lines += ["", "Open P0/P1 issues:", issues]
    else:
        lines += ["", "Open P0/P1 issues: none found (or gh not authenticated)."]
    lines += [
        "",
        "Reminder: delegate work per your project's `.claude/CLAUDE.md` agent roster - "
        "check it for any role that owns handling of sensitive or regulated data.",
    ]
    print("\n".join(lines))


if __name__ == "__main__":
    main()
