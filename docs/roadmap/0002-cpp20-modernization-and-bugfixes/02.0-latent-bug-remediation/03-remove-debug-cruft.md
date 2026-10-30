# 03 - Remove DEBUG cruft

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Remove the `// DEBUG` / `#include <iostream>` pair at the top of
`smbios.cpp` ([smbios.cpp:14-15](/smbios/src/smbios.cpp)), which CLAUDE.md
itself already flags as a known rough edge, and sweep for any other
leftover debug-only includes or commented-out code in the files this
milestone otherwise touches (`smbios.cpp`, `unix_bios.cpp`, `win_bios.cpp`).

## Files

- **Modify** `smbios/src/smbios.cpp`: remove the two lines; confirm nothing
  in the file actually uses `std::cout`/`std::cerr`/etc. after removal
  (`using std::numeric_limits;` and the rest of the `using` block stay -
  they are used).
- **Modify** `smbios/src/win_bios.cpp`: sweep for the equivalent pattern if
  present (not confirmed yet - verify during implementation).

## Functions and Classes

N/A.

## Constraints

- This is a pure deletion task - if removing the include breaks
  compilation, that means something *is* using `<iostream>` transitively
  and the task must investigate why before deleting (do not re-add the
  include defensively; find the real transitive dependency and include it
  explicitly where it's actually used).

## Success criteria

- [ ] `grep -n "// DEBUG" smbios/src/*.cpp` returns no matches.
- [ ] `smbios.cpp` compiles without `<iostream>`.
- [ ] CLAUDE.md's "Known rough edges" bullet about the `// DEBUG` include is
      removed (it is fixed, not merely known, after this task).
