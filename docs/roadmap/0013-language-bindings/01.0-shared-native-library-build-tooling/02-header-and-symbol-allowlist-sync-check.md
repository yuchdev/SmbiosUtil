# 02 - Header and symbol-allowlist sync check

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Add one script, run by both bindings' CI jobs, that parses the real
source of truth - `smbios_c/include/smbios/smbios.h`'s `extern`
declarations and `smbios_c/smbios_c.exported_symbols` /
`smbios_c/smbios_c.version`'s symbol lists - and fails if either
binding's own FFI declaration list (a `bindgen`-generated Rust file, or
a `cffi`/`ctypes` declaration string in Python) names a function not in
that list, or omits a function that is. This is the single mechanism
this milestone adds to stop the exact class of drift already seen once
in this project's own documentation (the `smbios.hpp`/`smbios.h` naming
mismatch corrected in milestone 0007's `status.md`), now applied to
generated/hand-written FFI code instead of prose.

## Files

- **Create** `scripts/check_c_abi_binding_sync.py`: parses
  `smbios_c/smbios_c.version`'s `global:` block (or
  `smbios_c.exported_symbols`'s flat list on macOS - both must already
  agree, per milestone 0007 Story 03.0's own verification) as the
  canonical 13-name symbol set, then checks each registered binding
  source (a configurable list of file globs, one entry per binding, added
  by Stories 02.0/03.0 when their own generated/declared symbol lists
  exist) for an exact set match - no extra names, no missing names.
  Exits non-zero with a clear diff-style message (`+` for an extra name
  the binding declares but `smbios_c` doesn't export, `-` for a name
  `smbios_c` exports that the binding is missing) on any mismatch.

## Functions and Classes

- `check_c_abi_binding_sync.py` - new script; no production library code.
  Suggested shape (names only, not an implementation):
  - `load_canonical_symbol_set(version_script_path) -> set[str]`
  - `load_binding_symbol_set(binding_name, glob_patterns) -> set[str]`
  - `main() -> int` (exit code; prints the diff)

## Constraints

- The script must have **zero knowledge of Rust or Python syntax beyond
  a simple regex/text extraction** of exported symbol names from
  whatever declaration shape each binding settles on in Stories
  02.0/03.0 (e.g. a `bindgen`-emitted `extern "C" { fn smbios_open(...); }`
  block, or a Python `ffi.cdef(...)` string) - it is not a full parser for
  either language, only a name-extraction pass, to keep it maintainable
  by whoever touches either binding later, not just a Python/Rust
  specialist.
- Must run with no `smbios_c` build required (it only reads
  `smbios_c.version`/`smbios_c.exported_symbols` as static text, not a
  built artifact) - so it is cheap enough to run on every CI job, not
  just a nightly one.
- Must be added to `scripts/` alongside this project's existing
  `scripts/check_doc_links.py` and `scripts/check_doc_samples.py`,
  following their existing CLI-invocation convention (a plain script, no
  new third-party Python dependency beyond what those two already use,
  if any).

## Tests

- A fixture pair under `tests/data/c_abi_binding_sync/` (or similar): one
  "matches" fixture (a binding declaration list that exactly mirrors the
  real 13 symbols) and at least two "drift" fixtures - one with an extra
  undeclared name, one missing a real exported name - asserting the
  script exits 0 on the first and non-zero with the correct diff on each
  of the other two.

## Success criteria

- [ ] The script correctly parses the real `smbios_c.version`/
      `smbios_c.exported_symbols` files as of this milestone's start (13
      names, matching exactly).
- [ ] The script fails clearly on each of the two drift fixtures above.
- [ ] The script requires no `smbios_c` build to run.
