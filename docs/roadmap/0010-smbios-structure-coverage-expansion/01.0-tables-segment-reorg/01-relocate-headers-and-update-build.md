# 01 - Relocate Headers and Update Build

**Parent story:** [README.md](README.md)
**Status:** ✅ Done

## Objective

Move the 11 existing entry headers into
`libsmbios/include/smbios/tables/`, update every `#include` site, and
update the one `CMakeLists.txt` glob line that lists headers - per ADR
0002's Decision (points 1-4).

## Files

- **Move** (`git mv`, preserving history) each of:
  `libsmbios/include/smbios/bios_information_entry.h`,
  `system_information_entry.h`, `system_enclosure_entry.h`,
  `processor_information_entry.h`, `cache_information_entry.h`,
  `port_connection_entry.h`, `system_slots_entry.h`,
  `physical_memory_array_entry.h`, `memory_device_entry.h`,
  `memory_array_mapped_address_entry.h`,
  `system_boot_information_entry.h` -> the same filename under
  `libsmbios/include/smbios/tables/`.
- **Modify** each of the corresponding 11 `.cpp` files in `libsmbios/src/`
  (unchanged location): update their own `#include <smbios/xxx_entry.h>`
  to `#include <smbios/tables/xxx_entry.h>`.
- **Modify** `libsmbios/src/CMakeLists.txt`: the header glob line (ADR
  0002's Decision point 4, exact replacement text given there).
- **Modify** `smbios/src/main.cpp` / `command_line_parser.cpp`: update any
  `#include <smbios/xxx_entry.h>` to the new `tables/` path.
- **Modify** every `tests/unit/*_entry_test.cpp` that includes an entry
  header directly (grep `#include <smbios/.*_entry\.h>` across `tests/` to
  find the complete list before starting - do not rely on this task spec's
  own enumeration being exhaustive, since test files were not individually
  audited while writing this spec).

## Functions and Classes

None added, removed, or signature-changed. This task touches `#include`
directives and one CMake glob line only.

## Constraints

- No entry class's constructor signature, accessor names, or
  `render_to_description()` output format changes as part of this task -
  any such change is out of scope and should be flagged, not folded in
  here.
- Confirm (by grepping the 11 moved headers' own `#include` lists before
  moving them) that none of them includes a sibling entry header - if one
  does, its own `#include` also needs the `tables/` prefix added at move
  time.
- Do not touch `smbios_entry_factory.h`/`.cpp` (ADR 0002 point 5) or any of
  the 4 non-entry headers staying flat in `include/smbios/`
  (`smbios.h`, `abstract_smbios_entry.h`, `smbios_entry_factory.h`,
  `smbios_entry_interface.h`).

## Tests

Covered by Task 02 (full-suite regression run) - this task does not add
new tests of its own, since it changes no behavior to test.

## Success criteria

- [x] All 11 headers exist only under `libsmbios/include/smbios/tables/`;
      no stale copy remains at the old flat path.
- [x] `cmake --build` succeeds cleanly on at least one platform with no new
      warnings.
- [x] `grep -rn '#include <smbios/[a-z_]*_entry\.h>' .` (excluding the new
      `tables/` directory itself) returns zero matches outside
      `libsmbios/include/smbios/tables/` and `libsmbios/src/` (whose `.cpp`
      files now say `#include <smbios/tables/...>`).
