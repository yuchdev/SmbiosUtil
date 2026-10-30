# 01 - Define interface and chain

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement contract **C1** (`ISmbiosSource`) and contract **C2** (the
source chain), and resolve the open question in the milestone plan (R8)
about how much `#if defined(_WIN32)` can be removed from `smbios.cpp`.

## Files

- **Create** `smbios/include/smbios/ismbios_source.h` per contract C1.
- **Create** `smbios/include/smbios/smbios_source_chain.h` / `.cpp`:
  ```cpp
  std::vector<std::unique_ptr<ISmbiosSource>> make_default_source_chain();
  ```
  with the actual list of sources built per-platform behind a minimal
  `#if defined(_WIN32)` / `#elif defined(__APPLE__)` / `#else` - this is
  the resolution of R8: the `#if` shrinks to "which source types exist on
  this platform," while `SMBios`'s own constructor logic (iterate the
  chain, call `is_available()` then `try_read()`, stop at first success)
  is platform-neutral C++ with no `#if` at all.
- **Modify** `smbios/include/smbios/smbios.h` / `smbios/src/smbios.cpp`:
  constructor becomes
  ```cpp
  SMBios::SMBios() {
      for (auto& source : make_default_source_chain()) {
          if (source->is_available() && source->try_read(table_buffer_, major_version_, minor_version_)) {
              source_name_ = source->source_name();
              break;
          }
      }
      read_smbios_table();
  }
  ```
  (exact member names to be reconciled with the existing
  `entry_point_buffer_`/`checksum_validated_` members during
  implementation - some may move into `LegacyMemoryScanSource` entirely,
  per Task 02).

## Functions and Classes

- `smbios::ISmbiosSource` - new abstract interface.
- `smbios::make_default_source_chain()` - new factory function.
- `SMBios::SMBios()` - rewritten.

## Constraints

- No real new source exists yet in this task - `make_default_source_chain()`
  on every platform returns a chain of exactly one element (the existing
  native impl, or the legacy scan, matching today's behavior) until
  Stories 02.0-04.0 add real entries.
- `win_bios.h`'s existing `SMBiosImpl` is **not** deleted in this task -
  Story 04.0 adapts it to implement `ISmbiosSource` directly; until then,
  a thin wrapper (`WindowsNativeSource` delegating to the existing
  `SMBiosImpl`) is an acceptable intermediate shape if it unblocks this
  story without touching Windows-specific code before Story 04.0's turn.

## Tests

- `tests/unit/smbios_source_chain_test.cpp` (new): a fake `ISmbiosSource`
  that reports unavailable, chained before one that succeeds - confirm the
  chain tries the first, skips to the second, and stops there (does not
  try a third source that would also succeed).

## Success criteria

- [ ] `ISmbiosSource` compiles and is implementable.
- [ ] `SMBios`'s constructor on every current platform produces
      byte-identical results to before this task (no real source changed
      yet).
- [ ] The fake-chain test passes.
