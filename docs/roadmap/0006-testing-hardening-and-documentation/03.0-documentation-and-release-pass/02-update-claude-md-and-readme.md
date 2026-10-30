# 02 - Update CLAUDE.md and README

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01 (architecture doc to cross-reference)

## Objective

Rewrite CLAUDE.md's "Overview," "Build and test," and "Architecture"
sections against the as-built system (R6), and correct the root README's
platform-support claims (R7).

## Files

- **Modify** `CLAUDE.md`:
  - "Overview": C++20 (not "C++11/`gnu++1y`"); vcpkg + CMake presets (not
    manual Boost paths); Windows/Linux/**macOS** are now all real,
    UEFI-capable targets (not "Mac has no native SMBIOS source").
  - "Build and test": the `cmake --preset <os>` workflow (milestone
    0001); the hermetic unprivileged suite alongside the real-hardware
    one (milestone 0006, Story 01.0); the fuzz harness's local-only
    workflow (milestone 0006, Story 02.0).
  - "Architecture": the `ISmbiosSource` chain (milestone 0005) replacing
    "Platform selection is done by compile-time `#if` on the header";
    the field-table accessor pattern (milestone 0003) replacing the
    description of versioned struct inheritance; the self-registering
    factory (milestone 0004) replacing the "map SMBIOS structure type to
    a Boost.Factory generator" description; removal of the "Known rough
    edges" bullets this roadmap fixed (the `SMBiosVersion` operator bug,
    the `// DEBUG` include - both milestone 0002).
- **Modify** root `README.md`: remove/correct "Mac has no native SMBIOS
  source (README asks for a UEFI implementation)" - the UEFI
  implementation this line asked for is what milestone 0005 delivered.

## Functions and Classes

N/A - documentation only.

## Constraints

- CLAUDE.md is rewritten wholesale for these sections (R6), not edited
  line-by-line against the old text - verify the result reads as a
  coherent description of the finished system, not as a diff.

## Tests

- A fresh read-through by someone unfamiliar with this roadmap (or a
  careful self-review) confirming CLAUDE.md alone (without reading this
  roadmap) correctly describes how to build, test, and extend the project
  as it exists after milestone 0006.

## Success criteria

- [ ] CLAUDE.md's "Overview"/"Build and test"/"Architecture" sections
      match the as-built system with no stale references to removed
      patterns (`#if _WIN32` dispatch, versioned structs, hand-wired
      factory, manual Boost paths).
- [ ] README's platform-support statement is corrected.
- [ ] `scripts/check_doc_links.py docs/` (or `/link-check`) reports zero
      problems across the entire roadmap.
