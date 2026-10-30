---
name: doc-xref
description: User-invoked as /doc-xref <target>. Finds every inbound reference to a doc path, heading/anchor, or code symbol - across docs/ AND src/ docstrings and comments - so a rename, move, split, or reworded section propagates everywhere. Use before renaming/moving/splitting docs or public symbols.
allowed-tools: Grep, Glob, Read, Edit, Bash
invocation: /doc-xref <target>
---

# Doc Cross-Reference

Find and update everything that points **at** `$ARGUMENTS` (a doc path, a heading
text/anchor, or a public symbol).

This is the **inbound** direction, and it is the only one that still works after the
target is renamed or deleted - at that point `/link-check` has nothing left to check.
`/link-check` covers the opposite direction (does what a file points at resolve?).
Run both after a rename.

Unlike `/link-check`, this skill also searches **`src/**` and `tests/**`**, because a
renamed doc or symbol is just as likely to be referenced from a docstring or comment
as from another Markdown file.

[examples/rename-propagation.md](examples/rename-propagation.md) is a full worked
sweep (rename a spec + a heading, update every inbound hit across docs and code).

## Steps

1. **Prerequisite — linkify bare mentions** so prose references become discoverable
   links before the inbound search runs:
   ```bash
   python scripts/linkify_doc_mentions.py docs .claude
   ```
   Note this **rewrites files**. Pass the narrowest roots that could plausibly
   reference the target rather than letting it sweep the whole corpus - a "find
   references" run should not quietly reformat unrelated docs. Unresolvable mentions
   land in `.claude/state/linkify-report.md`.

2. Enumerate inbound references across both trees:
   - **Docs**: links `](<path-or-anchor>)`, bare path mentions, and prose mentions
     of the heading/title in `docs/**`, repo-root `*.md`, and `.claude/**`.
   - **Code**: docstring / comment pointers in `src/**` and `tests/**` (e.g.
     ``See docs/adr/template.md#alternatives-considered``), plus the symbol name itself
     when the target is a symbol.
   ```bash
   git grep -nF "<target>"   # exact path / anchor / symbol; repeat for old name + anchor
   ```

3. **Rename/move**: update every hit to the new path/anchor; `git mv` when renaming
   a whole file so history follows.

4. **Reworded section**: update mentions whose surrounding text now misstates the
   section (registry descriptions, "see X" summaries, titles).

5. Update the `docs/README.md` registry line if a file was added/renamed/removed.

6. Verify with `/link-check` (or `python scripts/check_doc_links.py`).

## Output

List each reference updated (`file:line`, old → new) and confirm `/link-check`
passes. Flag any reference you could not safely auto-update for human review.

## Completion checklist

- [ ] Every hit from `git grep` is updated - file:line list provided in output
- [ ] `src/` and `tests/` searched, not just Markdown
- [ ] Any reference not safely auto-updated is explicitly flagged for human review
- [ ] `docs/README.md` registry line updated if the file was renamed or moved
- [ ] `/link-check` exits 0 after all edits
