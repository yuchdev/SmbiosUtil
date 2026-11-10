# Story 02.0 - Rust Crates (`smbios-sys` + `smbios`)

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** `smbios_c` / milestone 0007 (the C ABI this story wraps);
Story 01.0 (native-library acquisition strategy, sync check)

## Scope

Two crates, the standard Rust split for a C-backed library (the same
shape as, e.g., `libgit2-sys`/`git2`, `openssl-sys`/`openssl`):

1. **`smbios-sys`** - raw, `unsafe`, `bindgen`-generated FFI bindings,
   one-to-one with `smbios_c/include/smbios/smbios.h`'s 13 exported
   functions and its opaque handle/enum types. No safety wrapper, no
   RAII, no `Result` - a direct, mechanical mirror of the C header, the
   same role `smbios_c`'s own header plays for a C caller.
2. **`smbios`** - a thin, idiomatic, safe wrapper crate depending on
   `smbios-sys`: RAII `Drop` impls for both opaque handles, a
   `Result<T, SmbiosError>` return convention instead of the raw
   `smbios_error_t` enum, and a safe iterator over entries.

## Tasks

| #  | Document                                                        | Status         | Blocks |
|----|------------------------------------------------------------------|----------------|--------|
| 01 | [`smbios-sys` raw bindgen crate](01-smbios-sys-raw-bindgen-crate.md) | ⬜ Not started | 02     |
| 02 | [`smbios` safe wrapper crate](02-smbios-safe-wrapper-crate.md)       | ⬜ Not started | 03     |
| 03 | [crates.io packaging and publishing](03-crates-io-packaging-and-publishing.md) | ⬜ Not started | 04     |
| 04 | [Tests](04-tests.md)                                                  | ⬜ Not started | -      |

## Key constraints

- `smbios-sys` never adds safety or convenience beyond what `bindgen`
  itself generates from `smbios.h` - any ergonomics (RAII, `Result`,
  iterators) belong in the `smbios` wrapper crate only, matching the
  `-sys`/safe-wrapper split's own established Rust convention, so a
  caller who genuinely wants raw `unsafe` control (e.g. to minimize
  binary size, or to build a different safety abstraction) still has a
  minimal-dependency option.
- Every `smbios-sys` function name matches
  `smbios_c/smbios_c.exported_symbols`/`smbios_c.version`'s 13 names
  exactly (enforced by Story 01.0 Task 02's sync check) - no renaming,
  even where Rust naming convention would prefer
  `snake_case`-without-`smbios_`-prefix (that renaming, if wanted, is the
  safe wrapper crate's job, via its own idiomatic method names, not
  `smbios-sys`'s).
- `smbios_entry_handle_t`'s documented lifetime rule (non-owning view,
  valid only until the parent `smbios_handle_t`'s `smbios_close()`,
  per milestone 0007 Story 02.0 Task 01) must be encoded in the safe
  wrapper as a Rust lifetime parameter tied to the parent handle's own
  borrow (e.g. an entry type structurally borrowing `&'a SmbiosHandle`),
  not merely documented in a comment - this is exactly the class of bug
  Rust's borrow checker can statically prevent that C/C++ cannot, and is
  the main reason this milestone bothers with a *safe* wrapper crate at
  all rather than stopping at `smbios-sys`.
- No getter beyond `smbios_c`'s existing curated five (BIOS
  vendor/version, System UUID, Memory Device size/speed) plus entry
  iteration/render is wrapped (plan.md's Key constraints) - a wrapper
  method for a field `smbios_c` does not expose is out of scope; note it
  as a follow-up instead (plan.md's Key constraints, same rule, applied
  here).

## Files or directories created or modified by this story

- `bindings/rust/smbios-sys/{Cargo.toml,build.rs,src/lib.rs}` (new).
- `bindings/rust/smbios/{Cargo.toml,src/lib.rs,src/error.rs,src/entry.rs}` (new).
- `docs/dev/language-bindings.md` (Story 01.0) - cross-linked, not
  duplicated.
