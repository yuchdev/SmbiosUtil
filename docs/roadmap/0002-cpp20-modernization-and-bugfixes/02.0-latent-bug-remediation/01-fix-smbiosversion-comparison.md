# 01 - Fix SMBiosVersion comparison

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Replace `smbios::operator>` and `smbios::operator<` for `SMBiosVersion`
([smbios.cpp:20-40](/smbios/src/smbios.cpp)) with a correct lexicographic
comparison: major version is decisive unless equal, in which case minor
version decides. The current implementation checks `major_version >
rhs.major_version` and **independently** `minor_version > rhs.minor_version`
as two unconditional `if`s - so, for example, `SMBiosVersion{2,9} >
SMBiosVersion{3,0}` incorrectly returns `true` (the minor check `9 > 0`
fires even though major `2 < 3` should make the whole comparison false).

## Files

- **Modify** `smbios/src/smbios.cpp`:
  ```cpp
  bool smbios::operator>(const SMBiosVersion& lhs, const SMBiosVersion& rhs)
  {
      if (lhs.major_version != rhs.major_version) {
          return lhs.major_version > rhs.major_version;
      }
      return lhs.minor_version > rhs.minor_version;
  }

  bool smbios::operator<(const SMBiosVersion& lhs, const SMBiosVersion& rhs)
  {
      if (lhs.major_version != rhs.major_version) {
          return lhs.major_version < rhs.major_version;
      }
      return lhs.minor_version < rhs.minor_version;
  }
  ```

## Functions and Classes

- `smbios::operator>(const SMBiosVersion&, const SMBiosVersion&)` - fixed.
- `smbios::operator<(const SMBiosVersion&, const SMBiosVersion&)` - fixed.

## Constraints

- Every existing call site that compares an `SMBiosVersion` against a
  literal (`version > SMBiosVersion{2,3}` in `bios_information_entry.cpp`
  and `memory_device_entry.cpp`) must be re-checked against the corrected
  semantics - the fix may change which version-gated branch a given
  `(header.length, version)` pair now takes. Re-run the existing functional
  test suite (real SMBIOS table on a real/admin-capable machine) to confirm
  no entry's rendered output changes on the CI matrix's actual hardware.
- Do not add `operator==`/`operator!=` in this task - the existing code
  comment ("== != does not make sense, SMBIOS is backward compatible") is a
  deliberate design note from the original author and stays accurate; only
  `>`/`<` were buggy.

## Success criteria

- [ ] `SMBiosVersion{2,9} > SMBiosVersion{3,0}` is `false`.
- [ ] `SMBiosVersion{3,0} > SMBiosVersion{2,8}` is `true`.
- [ ] `SMBiosVersion{3,1} > SMBiosVersion{3,0}` is `true`;
      `SMBiosVersion{3,0} > SMBiosVersion{3,1}` is `false`.
- [ ] A new unit test exercising these three cases fails against the
      pre-fix operator and passes after.
