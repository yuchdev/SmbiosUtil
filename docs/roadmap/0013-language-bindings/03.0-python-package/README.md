# Story 03.0 - Python Package (`smbios`)

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** `smbios_c` / milestone 0007 (the C ABI this story wraps);
Story 01.0 (native-library acquisition strategy, sync check)

## Scope

One Python package, `smbios`, in two internal layers (not two separate
PyPI packages, unlike the Rust split - see Key constraints for why):

1. A low-level `smbios._ffi` module: raw `cffi` (or `ctypes` - decided in
   Task 01) declarations mirroring `smbios_c/include/smbios/smbios.h`'s
   13 exported functions and opaque handle/enum types, one-to-one, with
   no Pythonic convenience.
2. A public `smbios` package surface: a context-manager handle class, an
   exception hierarchy replacing `smbios_error_t` return codes, and an
   iterable/indexable entry collection - the Pythonic equivalent of the
   Rust wrapper crate's `Drop`/`Result` pair.

## Tasks

| #  | Document                                                          | Status         | Blocks |
|----|----------------------------------------------------------------------|----------------|--------|
| 01 | [`cffi` raw binding module](01-cffi-raw-binding-module.md)           | ⬜ Not started | 02     |
| 02 | [Pythonic wrapper](02-pythonic-wrapper.md)                           | ⬜ Not started | 03     |
| 03 | [Wheel packaging and PyPI publishing](03-wheel-packaging-and-pypi-publishing.md) | ⬜ Not started | 04     |
| 04 | [Tests](04-tests.md)                                                  | ⬜ Not started | -      |

## Key constraints

- **One PyPI package, not two**, unlike Rust's `-sys`/safe-wrapper split:
  Python has no equivalent ecosystem convention of shipping a
  deliberately-`unsafe`, low-level FFI package separately from its safe
  wrapper (Python itself has no `unsafe` keyword to delineate - the
  entire language is "unsafe" at the `ctypes`/`cffi` layer in the same
  sense), and `pip install smbios-sys` would not serve a distinct real
  audience the way `cargo add smbios-sys` does for a Rust caller
  building a different safety abstraction. The raw binding module
  (`smbios._ffi`) is a private implementation detail of the one public
  `smbios` package, not independently published.
- Every `smbios._ffi` declaration name matches
  `smbios_c/smbios_c.exported_symbols`/`smbios_c.version`'s 13 names
  exactly (enforced by Story 01.0 Task 02's sync check), same rule as
  the Rust story.
- `smbios_entry_handle_t`'s documented lifetime rule (non-owning view,
  valid only until the parent handle's `smbios_close()`) has no
  compiler-enforced equivalent in Python - it must instead be enforced
  **defensively, at runtime**: an `Entry` object retains a reference to
  its parent `Smbios` handle object and raises a clear `SmbiosError`
  (not a crash or undefined behavior) if used after the parent's
  `close()`/`__exit__` has run, by checking a parent-side "closed" flag
  before every native call. This is the Python story's equivalent of
  Rust's compile-time lifetime guarantee, deliberately weaker (runtime
  vs. compile-time) because Python itself has no mechanism for the
  stronger guarantee - name this difference explicitly in the package's
  own docstrings, not just in this spec.
- No getter beyond `smbios_c`'s existing curated five (BIOS
  vendor/version, System UUID, Memory Device size/speed) plus entry
  iteration/render is wrapped - same rule as Story 02.0.

## Files or directories created or modified by this story

- `bindings/python/pyproject.toml`, `bindings/python/src/smbios/{__init__.py,_ffi.py,_handle.py,_entry.py,_errors.py}` (new).
- `docs/dev/language-bindings.md` (Story 01.0) - cross-linked, not
  duplicated.
