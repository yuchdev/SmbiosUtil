---
name: link-check
description: User-invoked as /link-check [path ...]. Validates that documentation links and heading anchors resolve across docs/ (plus repo-root Markdown files and .claude/), using scripts/check_doc_links.py, and suggests rename candidates for missing files via scripts/doc_registry.py. Use after editing, splitting, merging, or renaming docs to catch dangling references.
allowed-tools: Bash, Read, Grep, Glob, Edit
invocation: /link-check [path ...]
---

# Link Check

Verify documentation cross-references resolve - every relative `[text](path)`,
`[text](path#anchor)`, and in-page `#anchor` points at an existing file and a real
heading. Backed by `scripts/check_doc_links.py`.

This is the **outbound** direction: does what a file points at resolve? For the
inbound direction - who points *at* a target you are about to rename - use
`/doc-xref <target>`. Run both after a rename.

Note on the automated gate: the `doc_link_check` hook runs on every edit and at
session end and checks the same *kinds* of defect, but it is a **separate, faster
implementation** (inline, importing `.claude/hooks/_common.py`) - it does not call
`scripts/check_doc_links.py`. The two deliberately agree on heading-slug rules; if
this skill and the hook ever disagree about one anchor, that is a bug in their
parity, not a judgement call.

## Steps

1. **Prerequisite — linkify bare mentions** (whole corpus, or scoped to `$ARGUMENTS`):
   ```bash
   python scripts/linkify_doc_mentions.py $ARGUMENTS
   ```
   This **rewrites files**, converting prose mentions of `*.md` filenames into
   Markdown links so the checker can see them at all. Any unresolvable mention is
   written to `.claude/state/linkify-report.md` for human review.

2. Run the checker over `$ARGUMENTS` (or the whole doc set when no path is given):
   ```bash
   python scripts/check_doc_links.py $ARGUMENTS
   ```

3. Fix each finding at the reported line:
   - **`missing anchor`** — update the anchor to the target's current GitHub-style
     heading slug (exact algorithm with worked examples in
     [references/anchor-slug-rules.md](references/anchor-slug-rules.md) — mind the
     double-hyphen case). If the target section has a hand-written
     `<a id="...">` anchor, prefer that stable name over the generated slug.
   - **`dangling link`** — the target file does not exist. If it looks like a typo
     or a moved file, get ranked rename candidates:
     ```bash
     python scripts/doc_registry.py
     ```
     Then act by tier (`[HIGH]` / `[MEDIUM]` / `[REVIEW]`) per
     [references/rename-candidates.md](references/rename-candidates.md). Fix the
     **link**, not the filename, when the correct file already exists elsewhere.

4. If a heading was renamed/moved, also run `/doc-xref <target>` to fix *inbound*
   links from other docs and from code docstrings - not just this file's outbound
   links.

5. Re-run step 2 until it exits `0`. A clean `doc_registry.py` run is **not**
   sufficient on its own: it only resolves file existence and never looks at
   anchors, so `check_doc_links.py` exiting 0 is the gate.

## Output

Report the findings fixed and confirm a clean `exit 0`. Note any link left
intentionally dangling (e.g. a planned-but-unwritten doc) so reviewers know it is
deliberate.

## Complement

| Tool                 | Direction | Checks                                                    |
|----------------------|-----------|-----------------------------------------------------------|
| `/link-check`        | outbound  | Link targets exist **and** anchors resolve (the gate)     |
| `/doc-xref <target>` | inbound   | References from other docs **and** `src/`/`tests/` comments |
| `/loop update-docs`  | both      | Iterative auto-fix + human review for the full corpus     |

## Completion checklist

- [ ] `python scripts/check_doc_links.py` exits 0 over the scoped paths
- [ ] Every `dangling link` either repointed, or flagged as deliberate in the output
- [ ] `/doc-xref` run for any heading or file that was renamed or moved
- [ ] Anything `.claude/state/linkify-report.md` could not resolve is surfaced to the user
