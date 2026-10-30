# Story 02.0 - Latent Bug Remediation

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (needs `-Werror` + sanitizers on to prove each fix)

## Scope

Fix the three concrete defects listed in the milestone plan's "Known
defects" section: the non-lexicographic `SMBiosVersion` comparison
operators, the missing `return` in `unix_bios.cpp`'s `get_table_size()`,
and the dishonest `*nix` support claims/dead `// DEBUG` include in
`smbios.cpp`. Each gets its own task and its own regression test.

## Tasks

| #  | Document                                                                  | Status         | Blocks |
|----|------------------------------------------------------------------------------|----------------|--------|
| 01 | [Fix SMBiosVersion comparison](01-fix-smbiosversion-comparison.md)            | ⬜ Not started | 04     |
| 02 | [Fix unix get_table_size and stub honesty](02-fix-unix-get-table-size-and-stub-honesty.md) | ⬜ Not started | 04     |
| 03 | [Remove DEBUG cruft](03-remove-debug-cruft.md)                                | ⬜ Not started | 04     |
| 04 | [Tests](04-tests.md)                                                          | ⬜ Not started | -      |

## Key constraints

- Each fix is scoped to the defect it names - no drive-by redesign of
  `SMBios`, `SMBiosImpl`, or the versioned-entry classes. The actual
  redesign of the versioning/parsing model is milestone 0003's job; this
  story only makes the *existing* comparison operator correct so milestone
  0003 can build on (or deliberately replace) a working baseline.
- Every fix ships with a unit test that fails against the pre-fix code and
  passes after - not just "builds clean under `-Werror`."

## Files or directories created or modified by this story

- `smbios/src/smbios.cpp` - `operator>`/`operator<` for `SMBiosVersion`;
  removes the `// DEBUG` `<iostream>` include.
- `smbios/src/unix_bios.cpp` - `get_table_size()` return statement;
  `sysfs_table_exists()`/class doc comment corrected to describe actual
  (not aspirational) behavior.
- `smbios/include/smbios/unix_bios.h` - class doc comment correction.
- `smbios/test/functional_test/func_tests.cpp` (or a new unit test target,
  if this story is the first to need one that doesn't require root) - new
  regression tests for all three fixes.
