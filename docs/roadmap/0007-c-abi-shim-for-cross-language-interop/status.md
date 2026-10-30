# Milestone 0007 - C ABI Shim for Cross-Language Interop - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                     | Status         | Tests |
|-------|---------------------------------------------|----------------|-------|
| 01.0  | C ABI core: handles and error model        | ⬜ Not started | -     |
| 02.0  | Entry and field accessors C API            | ⬜ Not started | -     |
| 03.0  | Build integration and packaging            | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Added at the user's request after a design discussion on how to offer
  both a C and a C++ interface. The architectural decision made then,
  carried into `plan.md`: a thin, separately-versioned C ABI shim over
  opaque handles, wrapping the existing C++ core - not a second C
  implementation, and not an attempt to make the C++ headers themselves
  C-compatible.
- Depends on milestone 0004 (entry model and factory finalized) landing
  first, so this shim wraps a settled surface rather than one still being
  redesigned by milestones 0002/0003.
- The decision to wrap only a curated subset of typed getters (not every
  C++ getter 1:1) is deliberate scope control, recorded in `plan.md`'s Key
  constraints - revisit and expand the curated set in a future milestone
  once real external consumers (a Python/Rust/C# binding) identify which
  fields they actually need.
