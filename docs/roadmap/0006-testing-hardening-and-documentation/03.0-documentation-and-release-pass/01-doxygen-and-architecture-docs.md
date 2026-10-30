# 01 - Doxygen and architecture docs

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Audit every public class, method, and free function declared under
`smbios/include/smbios/` for a complete Doxygen comment (R5) - not merely
present, but complete: a `@brief`, parameter/return documentation where
non-trivial, and a note of any non-obvious constraint (an offset, a
sentinel value, a length requirement) - and write a short architecture
overview tying milestones 0003 (field-table accessors) and 0005
(`ISmbiosSource` chain) together for a reader who was not in this
roadmap's design discussions.

## Files

- **Modify** every header under `smbios/include/smbios/` as needed - most
  already have good `@brief` comments (this codebase's existing style,
  e.g. `BiosInformationEntry`'s enum documentation, is already a good
  model to match); this task's job is finding and fixing the gaps, not
  starting from zero.
- **Create** `docs/architecture/overview.md` (or similar): the
  `ISmbiosSource` chain diagram (reusing/adapting milestone 0005's
  `plan.md` Mermaid dependency graph as a starting point, but drawn as an
  actual data-flow diagram, not a task dependency graph), and a short
  "how to add a new structure type" section walking through milestone
  0004's pattern (one class, one `REGISTER_SMBIOS_ENTRY` line, field
  accessors via `read_uN` + length guards) for a future contributor.

## Functions and Classes

N/A - documentation only.

## Constraints

- Doxygen comment style matches the existing codebase's convention
  exactly (`/// @brief ...` single-line and multi-line forms as already
  used) - this task does not introduce a different comment style.

## Tests

- `doxygen` (if configured) runs clean with no new warnings about
  undocumented public members, if a `Doxyfile`/CI doc-check exists;
  otherwise, a manual grep-based audit (`grep -B2 "^\s*\(virtual\|public:\)"`
  style heuristics) cross-checked by hand is acceptable.

## Success criteria

- [ ] Every public class/method/function under `smbios/include/smbios/`
      has a complete Doxygen comment.
- [ ] `docs/architecture/overview.md` exists and accurately describes the
      as-built system (cross-checked against the actual code, not just
      against this roadmap's plan files, which describe *intent*).
