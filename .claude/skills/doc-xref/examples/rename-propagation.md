# Worked example: propagating a heading rename

Illustrates a full `/doc-xref` inbound-reference sweep when renaming a heading.
Scenario: rename the `## Alternatives Considered` section in
`docs/adr/template.md` to `## Alternatives & Trade-offs Considered`.

## 1. Linkify bare mentions first

```bash
python scripts/linkify_doc_mentions.py
```
Turns prose mentions of `template.md` into real links so the grep below finds them.

## 2. Enumerate inbound references (both trees)

```bash
git grep -nF "Alternatives Considered"   # old heading text - docs + code
git grep -nF "#alternatives-considered"  # old anchor
git grep -nF "template.md"               # anything pointing at the file itself
```

Representative hits:

```
core/skills/adr-write/SKILL.md:33:  the sections: `Context`, `Decision`, `Alternatives Considered` (a table),
core/skills/adr-write/SKILL.md:46:  Structure matches `docs/adr/template.md` exactly (Context, Decision, Alternatives Considered table, ...)
core/skills/adr-write/SKILL.md:48:  `Alternatives Considered` table has at least the chosen option + one rejected alternative
core/skills/adr-write/references/template-guide.md:31:## Alternatives Considered
core/skills/adr-write/references/template-guide.md:61:An empty Alternatives table or an all-positive Consequences section - both signal
```

## 3. Update every hit (heading + anchor + prose)

| File:line | Old | New |
|-----------|-----|-----|
| core/skills/adr-write/SKILL.md:33 | `` `Alternatives Considered` (a table) `` | `` `Alternatives & Trade-offs Considered` (a table) `` |
| core/skills/adr-write/SKILL.md:46 | `Alternatives Considered table` | `Alternatives & Trade-offs Considered table` |
| core/skills/adr-write/SKILL.md:48 | `` `Alternatives Considered` table `` | `` `Alternatives & Trade-offs Considered` table `` |
| core/skills/adr-write/references/template-guide.md:31 | `## Alternatives Considered` | `## Alternatives & Trade-offs Considered` |
| core/skills/adr-write/references/template-guide.md:61 | `An empty Alternatives table` | `An empty Alternatives & Trade-offs table` |

- Note the new anchor `#alternatives--trade-offs-considered` - the `&` is
  dropped and its surrounding spaces each become a hyphen (double hyphen).
  See link-check's anchor-slug rules.
- The heading lives in `docs/adr/template.md` itself (the canonical source) -
  update it there first, then propagate to every doc that quotes or links the
  section name.

## 4. Update the registry line

Edit the `docs/README.md` index entry if the change affects how the template
is described there.

## 5. Verify

```bash
python scripts/check_doc_links.py     # or /link-check
```

Confirm exit 0. Flag any reference you could not safely auto-update (e.g. an
ambiguous prose mention) for human review rather than guessing.

## Complement

`/doc-xref` fixes **inbound** references (this sweep); `/link-check` validates the
**outbound** links of any file you edited. Run both after a rename.
