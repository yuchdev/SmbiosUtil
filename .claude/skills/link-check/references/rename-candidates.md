# Rename candidates: reading `scripts/doc_registry.py` output

`/link-check` runs `scripts/check_doc_links.py`, which reports *that* a link target is
missing but not *what it should have been*. When a `dangling link` finding looks like a
typo or a moved file, `scripts/doc_registry.py` re-scans the corpus and ranks candidate
targets by `difflib` stem similarity. That ranking is its only unique capability — for
detection alone the link checker is a superset (it also catches broken **anchors**, which
the registry scanner does not look at).

Representative output shape (not live output):

```
Missing .md References
======================

[HIGH] Likely rename
  docs/README.md:42
    link:      ](dev/coding-standards.md)          # typo'd path
    candidate: docs/dev/coding-standard.md          (score 0.97)

[MEDIUM] Possible rename
  docs/adr/0002-storage-layer.md:88
    link:      ](../specs/design.md)
    candidates:
      docs/specs/storage-design.md                  (score 0.61)
      docs/specs/ui-design.md                       (score 0.55)

[REVIEW] No candidates found
  .claude/skills/pr-review/SKILL.md:12
    link:      ](../../docs/reviews/pr-template.md)
    candidates: (none)

Summary
=======
Registry: 128 .md files
Links scanned: 341
Missing: 3   (HIGH 1 | MEDIUM 1 | REVIEW 1)
```

## How to act on each tier

- **[HIGH] Likely rename** — the correct file exists at the candidate path; the link was
  written wrong. Fix the *link* to point at the candidate (shortest correct relative
  path). **Do not rename the file.** Above: `dev/coding-standards.md` →
  `dev/coding-standard.md`.
- **[MEDIUM] Possible rename** — inspect the candidates. If one is obviously right,
  repoint the link; if genuinely ambiguous, flag it for human review rather than guess.
- **[REVIEW] No candidates found** — decide among: create the missing file (and add it to
  `docs/README.md` if it belongs there), delete the broken link, or repoint it to an
  existing related file.

## Scope note

The registry scanner only resolves `.md` **file existence**. It says nothing about
anchors, so a clean registry run is never sufficient on its own — always finish on
`scripts/check_doc_links.py` exiting 0, which is what `/link-check` gates on.
