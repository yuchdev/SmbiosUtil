# 02 - Pythonic wrapper

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Wrap `smbios._ffi` in the package's public surface: a context-manager
handle class, an exception hierarchy replacing `smbios_error_t`, and an
iterable entry collection with the five curated typed getters as plain
properties/methods - no raw error codes or buffer-length arguments ever
visible to a consumer of the published package.

## Files

- **Create** `bindings/python/src/smbios/_errors.py`:
  ```python
  class SmbiosError(Exception):
      """Base class for every error this package raises."""

  class NoTableFoundError(SmbiosError): ...
  class InvalidHandleError(SmbiosError): ...
  class InvalidArgumentError(SmbiosError): ...
  class UnknownError(SmbiosError): ...
  # SMBIOS_ERR_BUFFER_TOO_SMALL is never raised publicly - see Constraints.

  def _raise_for(code) -> None:
      """Raises the matching SmbiosError subclass for a non-OK
      smbios_error_t; returns None (no-op) for SMBIOS_OK."""
      ...
  ```
- **Create** `bindings/python/src/smbios/_handle.py`:
  ```python
  class Smbios:
      """Context manager wrapping smbios_handle_t.

      with Smbios() as table:
          for entry in table:
              print(entry.render())
      """
      def __init__(self) -> None: ...   # calls smbios_open; raises SmbiosError subclass on failure
      def __enter__(self) -> "Smbios": ...
      def __exit__(self, exc_type, exc, tb) -> None: ...  # calls smbios_close; idempotent
      def close(self) -> None: ...      # explicit close, usable without `with`; idempotent
      def __len__(self) -> int: ...     # smbios_entry_count
      def __getitem__(self, index: int) -> "Entry": ...  # smbios_entry_at
      def __iter__(self): ...           # yields Entry objects in order
  ```
- **Create** `bindings/python/src/smbios/_entry.py`:
  ```python
  class Entry:
      """A view into one SMBIOS structure. Must not be used after its
      parent Smbios handle is closed - raises InvalidHandleError if it
      is (see README's lifetime-safety note: this is a runtime check,
      not a compile-time guarantee, unlike the Rust wrapper's
      lifetime-typed equivalent)."""
      @property
      def structure_type(self) -> int: ...
      @property
      def length(self) -> int: ...
      @property
      def handle_id(self) -> int: ...
      def render(self) -> str: ...
      def bios_vendor(self) -> str: ...        # raises InvalidArgumentError on type mismatch
      def bios_version(self) -> str: ...
      def system_uuid(self) -> str: ...
      def memory_device_size(self) -> str: ...
      def memory_device_speed(self) -> str: ...
  ```
- **Modify** `bindings/python/src/smbios/__init__.py`: re-exports
  `Smbios`, `Entry`, and the `SmbiosError` hierarchy as the package's
  complete public surface.

## Functions and Classes

- `Smbios`, `Entry`, `SmbiosError` and its four public subclasses,
  `_raise_for` - new.

## Constraints

- The buffer-length retry protocol (`SMBIOS_ERR_BUFFER_TOO_SMALL` +
  `out_needed_len`) is handled entirely inside `render()`/the typed
  getters - a caller of this package's public API never passes or sees
  a buffer size. Mirrors the Rust wrapper's identical rule (Story 02.0
  Task 02).
- `Entry`'s parent-liveness check (plan.md/this story's README's runtime
  lifetime-safety note) must run before every native call `Entry` makes,
  not only at construction - a `Entry` object obtained before `close()`
  and used after it must raise `InvalidHandleError`, never crash the
  interpreter, on every platform.
- `Smbios.close()`/`__exit__` must be idempotent (safe to call twice),
  mirroring `smbios_close()`'s own documented no-op-on-already-closed
  behavior at the C layer.
- `__init__.py` must not import `smbios._ffi` or expose any `cffi`/
  `ctypes` type in its public surface - a consumer never needs to know
  which raw-binding technology Task 01 chose.

## Tests

- `with Smbios() as table:` succeeds on real hardware and `close()` is
  confirmed idempotent; using an `Entry` obtained from a closed `Smbios`
  raises `InvalidHandleError` rather than crashing (a dedicated
  regression test for the exact defensive check this task adds).
- Each of the four public `SmbiosError` subclasses is confirmed to map
  from the correct underlying `smbios_error_t` value (forcing each path,
  e.g. a mismatched-type typed-getter call for `InvalidArgumentError`).
- Deferred to Story 04.0: cross-language parity against the Rust
  wrapper's own output for the same real/synthetic source.

## Success criteria

- [ ] `Smbios`/`Entry` never expose a raw error code, buffer length, or
      `cffi`/`ctypes` type in their public API.
- [ ] Use-after-close on an `Entry` raises `InvalidHandleError`,
      verified by a dedicated test, on every platform Story 01.0 Task 03
      scoped as pass/fail gating.
- [ ] `Smbios.close()` is confirmed idempotent.
