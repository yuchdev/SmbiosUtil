# 02 - Curated typed field accessors

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Wrap exactly the following getters - chosen as the fields a C caller is
most likely to want without parsing `smbios_entry_render()`'s free-form
text - per `plan.md`'s explicit curation constraint. This list is the
whole scope of this task; expanding it is a future milestone's decision,
not an implicit extension of this one.

- `BiosInformationEntry::get_vendor_string()` / `get_version_string()`
- `SystemInformationEntry::get_uuid_string()` (milestone 0004)
- `MemoryDeviceEntry::get_device_size_string()` /
  `get_device_speed_string()`

## Files

- **Modify** `smbios_c/include/smbios_c/smbios_c.h`:
  ```c
  smbios_error_t smbios_bios_vendor(smbios_entry_handle_t, char* buf, size_t buf_len, size_t* out_needed_len);
  smbios_error_t smbios_bios_version(smbios_entry_handle_t, char* buf, size_t buf_len, size_t* out_needed_len);
  smbios_error_t smbios_system_uuid(smbios_entry_handle_t, char* buf, size_t buf_len, size_t* out_needed_len);
  smbios_error_t smbios_memory_device_size(smbios_entry_handle_t, char* buf, size_t buf_len, size_t* out_needed_len);
  smbios_error_t smbios_memory_device_speed(smbios_entry_handle_t, char* buf, size_t buf_len, size_t* out_needed_len);
  ```
  Each returns `SMBIOS_ERR_INVALID_ARGUMENT` if called on a
  `smbios_entry_handle_t` whose underlying type doesn't match (e.g.
  calling `smbios_bios_vendor()` on a Memory Device entry) - a caller must
  check `smbios_entry_type()` first, same discipline the C++
  `BiosInformationEntry` constructor's own type check already enforces.
- **Modify** `smbios_c/src/smbios_c.cpp`: each function `dynamic_cast`s
  the underlying `AbstractSMBiosEntry*` to its concrete type and returns
  `SMBIOS_ERR_INVALID_ARGUMENT` on a failed cast, then delegates to the
  existing C++ getter and the same buffer-length pattern as
  `smbios_entry_render()` (Task 01).

## Functions and Classes

- `smbios_bios_vendor`, `smbios_bios_version`, `smbios_system_uuid`,
  `smbios_memory_device_size`, `smbios_memory_device_speed` - new.

## Constraints

- No getter outside this explicit list is added in this task, even if it
  looks trivial to add while already touching this file - that is exactly
  the scope-creep this task's parent story's Key constraints warn against.
- `dynamic_cast` requires RTTI to be enabled for the `smbios` library
  target - confirm this is already the case (it should be; nothing in
  milestones 0001-0006 disables RTTI) rather than assuming it.

## Tests

- `tests/c_abi/smbios_typed_getters_test.cpp` (new): each of the five
  getters, called on real hardware against the correct entry type,
  matches its C++ equivalent's string output exactly; each, called
  against a mismatched entry type, returns `SMBIOS_ERR_INVALID_ARGUMENT`.

## Success criteria

- [ ] All five curated getters match their C++ equivalents exactly on
      real hardware.
- [ ] Each getter correctly rejects a mismatched entry type.
