# Story 01.0 - Shared Native-Library Build/Discovery Tooling

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** `smbios_c` / milestone 0007 (the C ABI itself); milestone
0012 (how a built `smbios_c` + header pair is packaged/published -
referenced here, not re-decided)

## Scope

Decide and document, once, the mechanism both Story 02.0 (Rust) and
Story 03.0 (Python) build on to answer "how does this binding's own
build step get a built `smbios_c` shared library plus `smbios.h`
header?" - and add a CI-enforced check that both bindings' hand- or
tool-generated FFI declarations stay byte-exact against the real
exported symbol list, so a drift between `smbios_c/include/smbios/smbios.h`
and either binding's declarations is a build failure, not a runtime
crash on a caller's machine. This story produces no binding code itself
- it produces the contract and the drift-detection check that Stories
02.0/03.0 both implement against.

## Tasks

| #  | Document                                                                  | Status         | Blocks |
|----|------------------------------------------------------------------------------|----------------|--------|
| 01 | [Native-library acquisition strategy](01-native-library-acquisition-strategy.md) | ⬜ Not started | 02     |
| 02 | [Header and symbol-allowlist sync check](02-header-and-symbol-allowlist-sync-check.md) | ⬜ Not started | 03     |
| 03 | [CI matrix and platform scope](03-ci-matrix-and-platform-scope.md)           | ⬜ Not started | 04     |
| 04 | [Tests](04-tests.md)                                                          | ⬜ Not started | -      |

## Key constraints

- This story decides **mechanism**, not policy already settled elsewhere:
  it does not re-litigate ADR 0004's vcpkg-vs-Conan-vs-central-registry
  decision (milestone 0012's own scope) - it only decides which of ADR
  0004's already-adopted mechanisms (self-hosted vcpkg port, GitHub
  Releases artifacts, or a vendored source build) each binding's own
  build tooling depends on, per plan.md's "Dependency on milestone 0012"
  section.
- The sync check (Task 02) must fail loudly (non-zero exit, named
  mismatch) on any difference between
  `smbios_c/smbios_c.exported_symbols`/`smbios_c.version`'s symbol list
  and either binding's own FFI declaration list - this is the single
  mechanism preventing the exact `smbios.hpp`-vs-`smbios.h` class of
  drift already seen once in this project's own docs (see
  `docs/roadmap/0007-c-abi-shim-for-cross-language-interop/status.md`'s
  2026-11-10 correction).
- No binding-specific code (`.rs`, `.py`) is written in this story - it
  is tooling/documentation plus one drift-checking script, consumed by
  both later stories.

## Files or directories created or modified by this story

- `docs/dev/language-bindings.md` (new) - the shared acquisition-strategy
  decision record and the sync-check's own usage docs, cross-linked from
  both Story 02.0/03.0 READMEs instead of being duplicated into each.
- `scripts/check_c_abi_binding_sync.py` (new) - the drift-detection
  script (Task 02).
- `.github/workflows/ci.yml` - a new job or job step running the sync
  check (Task 03).
