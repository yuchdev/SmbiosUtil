# Story 03.0 - Documentation and Release Pass

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (documents the final test strategy, so should follow it)

## Scope

Audit every public Doxygen comment across `smbios/include/smbios/` (R5),
rewrite CLAUDE.md to describe the system as it exists after milestones
0001-0005 rather than patch it incrementally (R6), and correct the root
README's platform-support statement (R7). This is the roadmap's final
story.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [Doxygen and architecture docs](01-doxygen-and-architecture-docs.md)   | ⬜ Not started | 02     |
| 02 | [Update CLAUDE.md and README](02-update-claude-md-and-readme.md)      | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                                  | ⬜ Not started | -      |

## Key constraints

- CLAUDE.md is **rewritten**, not incrementally patched (R6) - by this
  point five milestones of accumulated edits would make incremental
  patching error-prone; a clean rewrite against the as-built system,
  cross-checked against this roadmap's own `plan.md`/`status.md` files
  for accuracy, is the better approach.
- Every roadmap link added across milestones 0001-0006 is verified
  resolvable (`/link-check docs/roadmap` or
  `python scripts/check_doc_links.py docs/`, per this project's own
  linking convention) before this story - and the whole roadmap - is
  considered closed.

## Files or directories created or modified by this story

- Every header under `smbios/include/smbios/` - Doxygen audit.
- `CLAUDE.md` - rewritten.
- `README.md` (repo root) - platform-support statement corrected.
- `docs/architecture/` (new, if it does not already exist) - a short
  architecture overview documenting the `ISmbiosSource` chain and the
  field-table accessor pattern, cross-referenced from CLAUDE.md.
