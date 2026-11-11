# 01 - `cffi` raw binding module

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Story 01.0 (acquisition strategy, sync check)

## Objective

Decide `cffi` vs. stdlib `ctypes` (see Constraints) and implement the
private `smbios._ffi` module: a one-to-one declaration of `smbios_c`'s
13 exported functions and its opaque handle/enum types, loaded against
whichever shared library Story 01.0 Task 01's acquisition path
locates/bundles, with no Pythonic convenience added at this layer.

## Files

- **Create** `bindings/python/src/smbios/_ffi.py`: the raw binding
  declarations and the library-loading logic. Illustrative `cffi`
  `cdef` shape (the real declaration is generated or hand-written to
  match `smbios_c/include/smbios/smbios.h` exactly, not transcribed from
  this spec from memory):
  ```python
  from cffi import FFI

  _ffi = FFI()
  _ffi.cdef("""
      typedef struct smbios_handle_s* smbios_handle_t;
      typedef struct smbios_entry_handle_s* smbios_entry_handle_t;

      typedef enum {
          SMBIOS_OK = 0,
          SMBIOS_ERR_NO_TABLE_FOUND = 1,
          SMBIOS_ERR_INVALID_HANDLE = 2,
          SMBIOS_ERR_INVALID_ARGUMENT = 3,
          SMBIOS_ERR_BUFFER_TOO_SMALL = 4,
          SMBIOS_ERR_UNKNOWN = 99
      } smbios_error_t;

      smbios_error_t smbios_open(smbios_handle_t* out_handle);
      void smbios_close(smbios_handle_t handle);
      smbios_error_t smbios_entry_count(smbios_handle_t handle, size_t* out_count);
      smbios_error_t smbios_entry_at(smbios_handle_t handle, size_t index, smbios_entry_handle_t* out_entry);
      uint8_t smbios_entry_type(smbios_entry_handle_t entry);
      uint8_t smbios_entry_length(smbios_entry_handle_t entry);
      uint16_t smbios_entry_handle_id(smbios_entry_handle_t entry);
      smbios_error_t smbios_entry_render(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len);
      smbios_error_t smbios_bios_vendor(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len);
      smbios_error_t smbios_bios_version(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len);
      smbios_error_t smbios_system_uuid(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len);
      smbios_error_t smbios_memory_device_size(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len);
      smbios_error_t smbios_memory_device_speed(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len);
  """)

  def _load_library():
      """Locates the smbios_c shared library per Story 01.0 Task 01's
      chosen default (a bundled per-platform binary inside the wheel)
      plus its documented override (an environment variable pointing at
      a system-installed smbios_c)."""
      ...

  _lib = _load_library()
  ```

## Functions and Classes

- `_ffi`, `_lib` (module-private) - new.
- `_load_library()` - new.

## Constraints

- **Decide and record** `cffi` (API mode, requiring a C compiler at
  wheel-*build* time only, not at `pip install` time, since the wheel
  ships a pre-built extension module) versus stdlib `ctypes` (zero build
  dependency at all, including wheel-build time, but a slightly more
  verbose/error-prone manual declaration style and no `cdef`-string
  single-source-of-truth) - record the choice and its rationale in
  `bindings/python/README.md`. Either choice must still pass Story 01.0
  Task 02's sync check against the real exported symbol list.
- `_load_library()` must raise a clear, actionable Python exception (not
  a bare `OSError`/segfault) if no `smbios_c` shared library can be
  located by any of Story 01.0 Task 01's documented mechanisms - a
  caller on an unsupported platform or with a broken install must get a
  message naming what was tried and what to do next.
- This module is private (`smbios._ffi`, leading underscore) - never
  imported directly by a consumer of the published package; the public
  surface is Task 02's wrapper only.

## Tests

- A `_ffi` smoke test: `_lib.smbios_open`/`smbios_close` callable
  directly, confirming the raw binding loads and the native symbols
  resolve, on every platform Story 01.0 Task 03 scoped as pass/fail
  gating.
- Confirms `_load_library()`'s error message is produced (not a crash)
  when the shared library is deliberately hidden/renamed in a test
  fixture.

## Success criteria

- [ ] The `cffi`-vs-`ctypes` decision is made and recorded with
      rationale.
- [ ] `_ffi` loads and calls the real `smbios_c` library successfully on
      at least Linux and macOS.
- [ ] A missing/unlocatable native library produces a clear Python
      exception, not a crash.
