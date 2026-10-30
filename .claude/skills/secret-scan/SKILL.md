---
name: secret-scan
description: Scans files for hardcoded secrets, API keys, and passwords using pattern matching. Runs automatically on every Write/Edit via the secret_scan.py PreToolUse hook (which blocks on a hit); invoke manually as /secret-scan [paths] to sweep existing files on demand.
allowed-tools: Read, Grep, Glob, Bash
invocation: /secret-scan [paths]
---

# Secret Scan

Detect committed credentials. This skill shares its detection engine with the
`PreToolUse(Write|Edit|MultiEdit)` hook, so the manual sweep and the automatic
block use identical rules.

## How it works

- **Automatic (block)**: `.claude/hooks/secret_scan.py` runs before every
  Write/Edit, scans the pending content, and exits 2 (blocking the write) if a
  likely secret is found. Findings are logged to `.claude/logs/secret-scan.log`.
- **Manual (sweep)**: this skill scans files already on disk.

## Steps

1. Resolve targets from `$ARGUMENTS`. If empty, sweep changed files:
   `git diff --name-only` + `git diff --cached --name-only` + untracked.
2. Run the shared scanner:
   `python .claude/hooks/secret_scan.py <file> [<file> ...]`
3. For each hit, report `file:line: <type>: <excerpt>`. Never print the full
   secret value beyond the short excerpt the scanner emits.

## Detected shapes

AWS keys, Anthropic/OpenAI/Google API keys, GitHub & Slack tokens, private-key
blocks, Postgres URLs with embedded passwords, and generic `password=`/`token=`
assignments. Obvious placeholders (`example`, `${VAR}`, `your-…`, `changeme`)
are ignored to limit false positives.

The full pattern table, the complete placeholder allowlist, per-type remediation,
and the skipped-suffix list are in
**[references/pattern-catalog.md](references/pattern-catalog.md)** — kept in sync
with `.claude/hooks/secret_scan.py`.

## Output

```
## Secret Scan - <N files>
- <file:line>: <type>
**Result: CLEAN | N FINDING(S)**
```
On any finding: instruct the user to remove the secret, rotate it if it ever
reached a remote, and replace it with an env var / `${VAR}` reference (see
`.mcp.json`) or a secrets manager. Recommend `git filter-repo`/history rewrite
if it was already committed.

## Completion checklist

- [ ] Targets resolved from `$ARGUMENTS`, or changed+untracked files when empty
- [ ] Shared scanner (`.claude/hooks/secret_scan.py`) run - not an ad-hoc regex
- [ ] Each finding reported as `file:line: <type>: <short excerpt>` - full secret value never printed
- [ ] Placeholder/allowlisted lines correctly treated as clean (see [references/pattern-catalog.md](references/pattern-catalog.md))
- [ ] For real hits: remove + rotate + replace with `${VAR}`/secrets-manager advised; history rewrite recommended if already committed
- [ ] Result line emitted: `CLEAN` or `N FINDING(S)`
