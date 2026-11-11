# 03 - Wheel packaging and PyPI publishing

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 02

## Objective

Produce platform-specific wheels that bundle a prebuilt `smbios_c`
shared library (per Story 01.0 Task 01's decision for Python) so `pip
install smbios` works with no C/C++ toolchain on the end user's own
machine, and document/automate the release process to PyPI.

## Files

- **Create** `bindings/python/pyproject.toml`: build backend (e.g.
  `scikit-build-core`, `setuptools` with a custom `build_ext`, or a
  pure-`cffi`-API-mode build hook - decided alongside Task 01's
  `cffi`-vs-`ctypes` choice, since only `cffi` API mode needs a
  build-time compile step at all), package metadata (`name = "smbios"`,
  `license`, `classifiers` including supported Python versions and
  OSes).
- **Create** `bindings/python/MANIFEST.in` or the `pyproject.toml`
  equivalent: ensures the bundled `smbios_c` shared library (`.so`/
  `.dylib`/`.dll`, obtained via Story 01.0 Task 01's chosen mechanism at
  wheel-build time) is included as package data, with `auditwheel`
  (Linux)/`delocate` (macOS)/`delvewheel` (Windows) run as part of the
  build to vendor the library's own transitive shared-object
  dependencies into the wheel - the standard pattern for a native-
  extension Python package that must not assume the end user's machine
  has anything beyond a Python interpreter.
- **Create** a CI workflow step (likely via `cibuildwheel`, the de facto
  standard for this exact problem) building one wheel per
  platform/Python-version combination Story 01.0 Task 03 scoped as
  pass/fail gating.
- **Create** `bindings/python/RELEASING.md` (or fold into
  `docs/dev/language-bindings.md`): the release process, including the
  `smbios_c` version range each release was built/tested against,
  recorded in the package's own changelog - same convention as the
  Rust story's Task 03.

## Functions and Classes

N/A - packaging/process only.

## Constraints

- A wheel must be a genuine binary wheel (`platform` tag, e.g.
  `manylinux_x86_64`, `macosx_11_0_arm64`), never a `py3-none-any` wheel
  that silently fails at import time on a platform missing a system
  `smbios_c` - this package bundles the native library, it does not
  assume one is already installed, consistent with Story 01.0 Task 01's
  Python-side decision.
- An `sdist` (source distribution) is published alongside the wheels for
  completeness/auditability, but is **not** the primary installation
  path - its own `pip install` from source would require a local
  `smbios_c` build per Story 01.0 Task 01's documented local-build
  override; document this gap explicitly rather than letting it surface
  as a confusing build failure for a user who ends up on the `sdist`
  path unexpectedly (e.g. an unsupported platform/Python-version
  combination with no matching prebuilt wheel).
- Actual `twine upload`/PyPI publish is a manual, maintainer-triggered
  step (no automatic publish-on-merge), matching the Rust story's Task
  03 and this project's current single-maintainer scale.

## Tests

- `cibuildwheel` (or chosen equivalent) produces a wheel for each
  platform/Python-version combination in scope, and each wheel installs
  cleanly into a fresh virtual environment with no other dependency
  present (confirmed by a CI step that `pip install`s the just-built
  wheel artifact directly, not an editable/development install) and
  successfully imports `smbios` and opens a handle.
- `twine check` on both the wheel and `sdist` artifacts passes before
  any real publish.

## Success criteria

- [ ] At least one genuine binary wheel builds and installs cleanly in
      an isolated environment on each pass/fail-gating platform.
- [ ] The bundled `smbios_c` library's own transitive dependencies are
      vendored correctly (`auditwheel`/`delocate`/`delvewheel` pass).
- [ ] The release process and `smbios_c` version-range-tracking
      convention are documented.
- [ ] The `sdist`-path limitation (requires a local `smbios_c` build) is
      explicitly documented, not left to surface as a confusing failure.
