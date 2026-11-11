# 02 - `smbios` safe wrapper crate

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Wrap `smbios-sys`'s raw FFI in a safe, idiomatic Rust API: RAII `Drop`
for both opaque handles (never a manual `close()` a caller can forget or
double-call), a `Result<T, SmbiosError>` return convention replacing
every raw `smbios_error_t`, a safe iterator over entries, and safe
`String`-returning wrappers over every caller-provided-buffer function
(`smbios_entry_render`, the five curated getters) that handle the
`SMBIOS_ERR_BUFFER_TOO_SMALL` retry-with-bigger-buffer protocol
internally so no caller of this crate ever sees that C idiom directly.

## Files

- **Create** `bindings/rust/smbios/src/error.rs`:
  ```rust
  #[derive(Debug, Clone, Copy, PartialEq, Eq)]
  pub enum SmbiosError {
      NoTableFound,
      InvalidHandle,
      InvalidArgument,
      BufferTooSmall, // only ever surfaces as an internal retry signal; see Constraints
      Unknown,
  }
  impl std::error::Error for SmbiosError {}
  impl std::fmt::Display for SmbiosError { /* ... */ }
  ```
- **Create** `bindings/rust/smbios/src/lib.rs`:
  ```rust
  pub struct SmbiosHandle { /* wraps smbios_sys::smbios_handle_t; Drop calls smbios_close */ }

  impl SmbiosHandle {
      pub fn open() -> Result<Self, SmbiosError> { /* smbios_open + map error */ }
      pub fn entry_count(&self) -> Result<usize, SmbiosError> { /* smbios_entry_count */ }
      pub fn entry(&self, index: usize) -> Result<Entry<'_>, SmbiosError> { /* smbios_entry_at */ }
      pub fn entries(&self) -> EntryIter<'_> { /* safe iterator, Task's own lifetime design */ }
  }

  impl Drop for SmbiosHandle {
      fn drop(&mut self) { unsafe { smbios_sys::smbios_close(self.raw) } }
  }
  ```
- **Create** `bindings/rust/smbios/src/entry.rs`:
  ```rust
  pub struct Entry<'a> { /* smbios_entry_handle_t + PhantomData<&'a SmbiosHandle> */ }

  impl<'a> Entry<'a> {
      pub fn structure_type(&self) -> u8 { /* smbios_entry_type */ }
      pub fn length(&self) -> u8 { /* smbios_entry_length */ }
      pub fn handle_id(&self) -> u16 { /* smbios_entry_handle_id */ }
      pub fn render(&self) -> Result<String, SmbiosError> { /* smbios_entry_render, buffer-retry handled internally */ }
      pub fn bios_vendor(&self) -> Result<String, SmbiosError> { /* smbios_bios_vendor; InvalidArgument on type mismatch */ }
      pub fn bios_version(&self) -> Result<String, SmbiosError> { /* smbios_bios_version */ }
      pub fn system_uuid(&self) -> Result<String, SmbiosError> { /* smbios_system_uuid */ }
      pub fn memory_device_size(&self) -> Result<String, SmbiosError> { /* smbios_memory_device_size */ }
      pub fn memory_device_speed(&self) -> Result<String, SmbiosError> { /* smbios_memory_device_speed */ }
  }
  ```

## Functions and Classes

- `SmbiosHandle`, `Entry<'a>`, `EntryIter<'a>`, `SmbiosError` - new.
- `SmbiosHandle::open`/`entry_count`/`entry`/`entries` - new.
- `Entry::structure_type`/`length`/`handle_id`/`render`/`bios_vendor`/
  `bios_version`/`system_uuid`/`memory_device_size`/
  `memory_device_speed` - new.

## Constraints

- `Entry<'a>`'s lifetime parameter must make it a compile error to use an
  `Entry` after its parent `SmbiosHandle` is dropped - this is the
  concrete enforcement of the C API's documented-but-unenforced "entry
  handle is a non-owning view, valid only until `smbios_close()`" rule
  (milestone 0007 Story 02.0 Task 01's own Constraints). Confirm with a
  `compile_fail` doc-test (a `SmbiosHandle` dropped while an `Entry`
  borrowed from it is still live must fail to compile, not merely fail
  at runtime).
- `SMBIOS_ERR_BUFFER_TOO_SMALL` is an implementation detail of the
  caller-provided-buffer C idiom (grow the buffer and retry using
  `out_needed_len`) - this wrapper must never surface it to a caller of
  `render()`/the typed getters; those always either return `Ok(String)`
  or a *different* `SmbiosError` variant. It remains in the
  `SmbiosError` enum only because `SmbiosHandle::open`'s underlying
  `smbios_open` could theoretically be extended to return it in a future
  `smbios_c` version - document this explicitly rather than silently
  relying on "it never happens in practice."
- Each typed getter returns `Err(SmbiosError::InvalidArgument)` on a
  type-mismatched entry, exactly mirroring `smbios_c`'s own documented
  behavior (milestone 0007 Story 02.0 Task 02) - never panics, never
  silently returns an empty string.
- No `unsafe` block appears in any public function signature or any
  doc-test example - `unsafe` is confined to this crate's own internal
  FFI call sites.

## Tests

- `bindings/rust/smbios/tests/`: `SmbiosHandle::open()` on real hardware
  succeeds and `Drop` runs cleanly (no leak, verified via a debug
  allocator or Miri where FFI allows); a `compile_fail` test proving the
  lifetime constraint above; the five typed getters' happy and
  type-mismatch paths, matching `smbios_c`'s own documented behavior.
- Deferred to Story 04.0: cross-language parity and deeper boundary
  stress tests (double-close safety at the FFI layer itself, error-code
  injection) - this task's own tests are the wrapper's unit-level
  correctness, not the shared cross-binding suite.

## Success criteria

- [ ] `Drop` correctly closes the handle exactly once, with no
      double-free possible through the public API.
- [ ] The lifetime constraint preventing entry-outlives-handle is
      enforced at compile time, proven by a `compile_fail` test.
- [ ] Every curated getter and `render()` returns `Result`, never a raw
      `smbios_error_t`, and never panics on a type mismatch.
