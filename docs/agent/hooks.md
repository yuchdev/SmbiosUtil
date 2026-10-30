# Hooks Reference

| Hook | Trigger | Description |
|------|---------|-------------|
| `doc_link_check` | PostToolUse: Write|Edit|MultiEdit; Stop | PostToolUse (Write|Edit|MultiEdit) + Stop: documentation reference integrity. |
| `github_audit` | PostToolUse: mcp__github__.* | PostToolUse / mcp__github__* - audit trail for GitHub MCP calls. |
| `guard_bash` | PreToolUse: Bash | PreToolUse / Bash guard. |
| `secret_scan` | PreToolUse: Write|Edit|MultiEdit | Secret scanner - dual mode. |
| `session_start` | SessionStart | SessionStart hook - seed the session with live repo context. |
