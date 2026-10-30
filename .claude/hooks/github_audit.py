#!/usr/bin/env python3
"""PostToolUse / mcp__github__* - audit trail for GitHub MCP calls.

Logs every GitHub MCP tool invocation to .claude/logs/github-actions.log with a
timestamp, the tool name, and a compact, secret-free summary of its arguments.
This satisfies the audit-trail requirement: any agent action that reaches GitHub
leaves a durable record. Never blocks.
"""

from __future__ import annotations

from _common import allow, append_log, read_event, tool_input

# Argument keys worth recording (kept small and non-sensitive).
SUMMARY_KEYS = (
    "owner", "repo", "pull_number", "issue_number", "number",
    "branch", "ref", "state", "title", "path", "query", "sha", "name",
)


def main() -> None:
    event = read_event()
    tool = event.get("tool_name", "mcp__github__?")
    fields = tool_input(event)
    summary_parts = []
    for key in SUMMARY_KEYS:
        if key in fields and fields[key] not in (None, ""):
            value = str(fields[key]).replace("\n", " ")[:80]
            summary_parts.append(f"{key}={value}")
    summary = " ".join(summary_parts) if summary_parts else "(no summarised args)"
    append_log("github-actions.log", f"{tool} :: {summary}")
    allow()


if __name__ == "__main__":
    main()
