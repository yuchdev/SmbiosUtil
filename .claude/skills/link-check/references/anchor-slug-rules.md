# Heading → anchor slug rules

Backs the "update the anchor to the current GitHub-style heading slug" step of
`/link-check`. `scripts/check_doc_links.py` validates in-page `#anchor` links
against slugs derived from headings by this algorithm — match it exactly when
fixing an anchor by hand.

## The slug algorithm (GitHub-flavoured)

For a heading's text (after stripping the leading `#`s and surrounding spaces),
`slugify` in `scripts/check_doc_links.py` does exactly three regex operations:

1. `strip()` the ends, then convert to **lowercase**.
2. Delete every character that is **not** a word char, whitespace, or hyphen
   (`re.sub(r"[^\w\s-]", "")`) — punctuation (`.,:;!?()[]{}"'/\` etc.), `&`, em
   dashes `—`, and emoji are **removed**, not replaced.
3. Replace **each whitespace character** with a hyphen (`re.sub(r"\s", "-")`).
   Note: *each* space, not each run — two spaces become two hyphens.

Underscores `_` and existing hyphens are word/hyphen chars, so they survive.
There is **no** run-collapsing and **no** trailing-hyphen trimming — a heading
ending in removed punctuation after a space (e.g. `## Foo :`) yields a trailing
hyphen (`#foo-`).

## Worked examples

| Heading | Slug (`#…`) |
|---------|-------------|
| `## Detection signals` | `#detection-signals` |
| `## Triage (first 5 minutes)` | `#triage-first-5-minutes` |
| `### Timeout & failure handling` | `#timeout--failure-handling` |
| `## P0 — must test` | `#p0--must-test` |
| `## Config fields worth naming` | `#config-fields-worth-naming` |
| `## Step ordering: detect → verify` | `#step-ordering-detect--verify` |

Note the **double hyphen**: `&`, `—`, `:`, and `→` are removed, but the spaces
that surrounded them each become a hyphen, so `A & B` → `a--b`. This is the most
common hand-fix mistake — don't collapse it to a single hyphen.

## Duplicate headings

If two headings produce the same slug, GitHub appends `-1`, `-2`, … to the second
and later occurrences (in document order). `#foo`, `#foo-1`, `#foo-2`. The checker
follows the same disambiguation.

## Fixing procedure

1. Find the *current* heading text in the target file.
2. Derive the slug with the rules above.
3. Update the link's `#anchor` to match.
4. If the heading itself was renamed/moved, also run `/doc-xref <heading>` to fix
   **inbound** links from other docs and from `src/` docstrings — `link-check`
   only fixes the outbound anchors in the file you edited.
