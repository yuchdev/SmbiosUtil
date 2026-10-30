---
name: pr-review
description: User-invoked as /pr-review [PR number or branch]. Spawns the feature-reviewer and security-auditor agents in parallel, waits for both, and synthesises a single unified review comment with one merge verdict. Use before merging any PR or finishing a feature branch.
allowed-tools: Read, Grep, Glob, Bash, Agent
invocation: /pr-review [pr|branch]
---

# PR Review (parallel feature + security)

Run a unified review of the change identified by `$ARGUMENTS` (a PR number, a
branch, or - if empty - the current working-tree diff).

## Steps

1. **Resolve the diff.** If a PR number is given, fetch it via the `github` MCP
   (or `gh pr diff <n>`). If a branch, `git diff main...<branch>`. Otherwise use
   `git diff` of the working tree. Capture the file list and the diff.
2. **Fan out, in parallel** (single message, two Agent calls):
   - `feature-reviewer` - correctness, domain accuracy, conventions, tests.
   - `security-auditor` - threat model of any auth/secret/integration/payload
     surface in the diff.
   Pass each agent the diff scope and changed-file list.
3. **Wait for all**, then **synthesise** one comment.

## Unified output

```
## libbios PR Review - <PR/branch>
**Merge verdict: APPROVE | REQUEST_CHANGES | BLOCK (security)**

### Feature review (feature-reviewer)
<verdict + blocking issues>

### Security review (security-auditor)
<verdict + findings; CRITICAL ⇒ BLOCK>

### Combined blocking items
1. [file:line] …

### Non-blocking suggestions
- …
```

Rules: any CRITICAL security finding ⇒ overall **BLOCK** regardless of the
feature verdict. Any feature blocking issue ⇒ at least **REQUEST_CHANGES**. If
`--comment` is requested and a PR number is known, post the synthesis as a PR
comment via the `github` MCP.

If any required agent returns null (terminal error), default the overall verdict
to **REQUEST_CHANGES** and name the failed agent explicitly in the report. Never
emit APPROVE from an incomplete run.

## Completion checklist

- [ ] Both `feature-reviewer` and `security-auditor` returned a verdict (not null / timed out)
- [ ] Any CRITICAL security finding → overall verdict is BLOCK, not REQUEST_CHANGES
- [ ] Combined blocking items de-duplicated across all agent reviews
- [ ] If `--comment` and PR number known → synthesis posted as PR comment via `github` MCP
