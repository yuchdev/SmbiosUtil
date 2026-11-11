# 01 - Native-library acquisition strategy

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Choose, per binding, which of plan.md's three "Dependency on milestone
0012" options a default build uses to obtain a built `smbios_c` shared
library + `smbios.h` header pair, and record the choice (and its
fallback, if any) where both Story 02.0 and Story 03.0 can reference it
without re-deciding it independently. Rust and Python's own ecosystem
norms differ enough that the same option need not win for both.

## Files

- **Create** `docs/dev/language-bindings.md`: a short decision record
  covering, at minimum:
  - For Rust (`smbios-sys`'s `build.rs`): whether it (a) shells out to
    `pkg-config --cflags --libs smbios_c` first (reusing milestone
    0007 Story 03.0's already-verified mechanism, via the `pkg-config`
    crate) and falls back to (b) downloading a tagged GitHub Releases
    artifact for the host triplet (ADR 0004 option 3) or (c) vendoring a
    `cmake`-driven source build (e.g. via the `cmake` crate) when neither
    (a) nor (b) is available - and the exact probe order.
  - For Python (the `smbios` wheel): whether the published wheel bundles
    a prebuilt `smbios_c` shared library per-platform (the common
    `cibuildwheel`/`auditwheel`/`delocate` pattern other native-extension
    Python packages use, sourced from ADR 0004's GitHub Releases
    artifacts at wheel-build time, not at `pip install` time on the end
    user's machine) versus requiring a system-installed `smbios_c`
    discoverable via `pkg-config` or a user-set environment variable.
  - The explicit statement that neither choice requires or assumes ADR
    0004's deferred *central*-registry vcpkg/Conan submission - both
    options above only ever depend on mechanisms ADR 0004 already
    adopted (the self-hosted port and/or GitHub Releases).
  - A short note on what changes (and what documentation/CI needs
    updating) if milestone 0012 later promotes the self-hosted vcpkg
    port to a central `microsoft/vcpkg` submission - this story's
    decision should degrade gracefully, not require a rewrite, if that
    promotion happens later.

## Functions and Classes

N/A - a decision record, no code.

## Constraints

- Record the decision as a **default with a documented override**, not
  a hard requirement: a contributor building either binding locally from
  a source checkout (no published GitHub Release yet, no vcpkg registry
  configured) must still have a documented path to a working local build
  - most plausibly "point `PKG_CONFIG_PATH` (Rust) /
  `SMBIOS_C_LIB_DIR`-style env var (Python) at a local `cmake --preset
  <os> && cmake --build && cmake --install` run of this same
  repository."
- Do not assume Windows parity with Linux/macOS by default - ADR 0004's
  own rollout section notes Windows vcpkg-port triplet support is
  unconfirmed; state explicitly whether Windows is in or out of this
  milestone's exit criteria for each binding (see Story 01.0 Task 03 for
  the CI-level consequence of this choice).

## Tests

- N/A for this task itself (a decision record) - Task 02's sync check
  and Story 04.0's FFI boundary suite are what eventually exercise the
  acquisition path chosen here, once implementation lands in a future
  milestone pass.

## Success criteria

- [ ] `docs/dev/language-bindings.md` names one default acquisition
      mechanism per binding, with a documented local-build override.
- [ ] The decision explicitly states its relationship to ADR 0004's
      deferred central-registry promotion (degrades gracefully, does not
      require it).
- [ ] Windows is explicitly scoped in or out for each binding, not left
      ambiguous.
