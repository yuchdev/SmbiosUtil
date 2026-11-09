# 0002 - `tables/` segment for SMBIOS structure-type entry classes

> **Status:** Proposed
>
> **Date:** 2026-11-08
>
> **Supersedes:** _(none)_
>
> **Superseded by:** _(none)_

## Context

`libsmbios` currently implements 11 of the 49 SMBIOS structure types the
DMTF SMBIOS 3.9.0 specification (`docs/standard/DSP0134_3.9.0.md`) defines
(Types 0, 1, 3, 4, 7, 8, 9, 16, 17, 19, 32). Every one of those 11 entry
classes lives flat, directly under `libsmbios/include/smbios/*.hpp` and
`libsmbios/src/*.cpp`, alongside genuinely different kinds of files in the
same two directories: the acquisition/parsing core (`smbios.hpp`/`.cpp`), the
entry base class (`abstract_smbios_entry.hpppp`/`.cpp`), and the
self-registering factory (`smbios_entry_factory.hpp`/`.cpp`).

Milestone 0010 (`docs/roadmap/0010-smbios-structure-coverage-expansion/`)
is about to add ~34 more entry classes (Types 2, 11-15, 18, 20-31, 33-46,
126). Landing 34 more files flat in the same two directories as today's 11
would leave 45 single-purpose entry-class files indistinguishable, by
location alone, from the 4 files that are actually core infrastructure -
exactly the kind of flat-directory sprawl this repo has already decided,
elsewhere, not to tolerate: every other library in the tree
(`libsmbios_linux`, `libsmbios_macos`, `libsmbios_win`, `sys_interface`)
groups its own multi-file concern under a named subdirectory of
`include/smbios/`.

### What the rest of the repo already does (the precedent)

Reading `libsmbios_linux/` as the worked example:

- `libsmbios_linux/include/smbios/linux/linux_sysfs_source.hpppppp` and
  `linux_efi_source.hpp` - headers live under a `linux/` **segment
  subdirectory** of `include/smbios/`.
- `libsmbios_linux/src/linux_sysfs_source.cpp`,
  `linux_efi_source.cpp`, `linux_source_chain.cpp` - sources stay **flat**,
  directly in `src/`, no `src/linux/` subdirectory.
- `libsmbios_linux/src/CMakeLists.txt` uses a non-recursive
  `file(GLOB SOURCES *.cpp ...)` (confirmed identical in
  `libsmbios/src/CMakeLists.txt`, whose own comment states this is
  deliberate: "`file(GLOB)` is non-recursive, so this only ever picks up
  `*.cpp` sitting directly in `libsmbios/src/` itself").

`sys_interface` follows the same split
(`sys_interface/include/smbios/sys_interface/*.h`, flat `src/`... actually
header-only, no `src/` at all, but the include-side segmentation is
identical).

So this repo's real, consistent convention is: **`include/` gets a named
segment subdirectory per logical group of headers; `src/` stays flat**, one
directory, one non-recursive glob, no matter how many `.cpp` files that
directory holds. `libsmbios`'s own entry-class files are the *only* place
in the current tree that has neither - they are flat on both sides because
there was, until now, no second "kind" of header in `libsmbios/include/
smbios/` to distinguish them from.

### Decision drivers

- **DD1 - Consistency with the established repo-wide convention.** A
  second, different include/src segmentation scheme invented just for
  `libsmbios`'s entry classes, when every other library already uses the
  "segmented include/, flat src/" shape, would be a second pattern
  maintainers need to remember for no technical reason.
- **DD2 - Minimal build-system churn.** `libsmbios/src/CMakeLists.txt`'s
  `file(GLOB)` is deliberately non-recursive (its own comment says so); the
  reorg should change that one glob line as little as possible, not
  restructure the target's build logic.
- **DD3 - Zero behavior change.** This is a pure file-move plus include-path
  update. No entry class's parsing logic, accessor signature, or
  `render_to_description()` output should change as a side effect of the
  move - the existing functional/integration/unit suite must pass
  unmodified once the move lands (same bar ADR 0001's own Story 01.1
  rollout held itself to for its skeleton restructuring).
- **DD4 - No `#include` churn beyond the files that actually move.** Every
  `#include <smbios/xxx_entry.hpp>` site - `smbios_entry_factory.cpp`'s
  implicit discovery (it imports nothing; headers self-register via their
  own `.cpp`), `smbios_util`'s CLI, and every `tests/unit/*_entry_test.cpp`
  - must be updated to the new `tables/` path, but nothing else about those
  call sites should change.

## Decision

1. Every SMBIOS structure-type entry header moves to a new
   `libsmbios/include/smbios/tables/` segment subdirectory:
   `bios_information_entry.hpp`, `system_information_entry.hpp`,
   `system_enclosure_entry.hpp`, `processor_information_entry.hpp`,
   `cache_information_entry.hpp`, `port_connection_entry.hpp`,
   `system_slots_entry.hpp`, `physical_memory_array_entry.hpp`,
   `memory_device_entry.hpp`, `memory_array_mapped_address_entry.hpp`,
   `system_boot_information_entry.hpp`, plus every new entry header milestone
   0010 adds. Call sites change from `#include <smbios/xxx_entry.hpp>` to
   `#include <smbios/tables/xxx_entry.hpp>`.
2. The corresponding `.cpp` files **stay flat** in `libsmbios/src/` -
   matching `libsmbios_linux`'s own src-side precedent exactly. No
   `src/tables/` subdirectory is created. The `.cpp` files' own
   `#include <smbios/tables/xxx_entry.hpp>` directive is the only change each
   one needs.
3. `libsmbios/include/smbios/*.hpp` keeps exactly the 4 files that are not
   per-structure-type entry classes: `smbios.hpp` (acquisition/parsing core,
   `SMBiosHandler` enum, `DMIHeader`, `SMBiosVersion`),
   `abstract_smbios_entry.hpppp` (the base class every `tables/` header
   derives from), `smbios_entry_factory.hpp` (the self-registering factory),
   `smbios_entry_interface.hpp` (`SMBiosInterface`). These stay at the flat
   root because they are not entry-class headers, not because of an
   oversight - `tables/` is a segment *for structure-type entries
   specifically*, not a dumping ground for everything in `libsmbios`.
4. `libsmbios/src/CMakeLists.txt`'s header glob changes from
   ```cmake
   file(GLOB SOURCES *.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../include/smbios/*.hpp)
   ```
   to
   ```cmake
   file(GLOB SOURCES *.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/smbios/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/smbios/tables/*.hpp)
   ```
   (The header glob entries exist for IDE/project-file visibility only -
   `add_library()` does not need headers listed to compile - so this change
   has no effect on what actually gets compiled; `*.cpp` is unaffected
   since every `.cpp` stays in `src/` directly per point 2.)
5. `smbios_entry_factory.hpp`/`.cpp` need **no edit** - the self-registering
   `REGISTER_SMBIOS_ENTRY` macro (ADR-adjacent design from milestone 0004)
   already means the factory never names a concrete entry class or its
   header; moving 45 entry headers changes zero lines in the factory
   itself. This is the same "zero-edit-to-add-a-type" property the factory
   was built for, now also paying off for a bulk relocation.

This reorg is milestone 0010's Story 01.0 (see
[docs/roadmap/0010-smbios-structure-coverage-expansion/plan.md](/docs/roadmap/0010-smbios-structure-coverage-expansion/plan.md)),
ordered first so every subsequent story's new entry classes land directly
in `tables/` instead of being written flat and moved again later.

## Alternatives Considered

| Alternative | Pros | Cons | Reason rejected |
|-------------|------|------|-----------------|
| **A - Status quo**: leave all entry headers/sources flat, let the ~34 new types land flat too | Zero migration work | 45 single-purpose files indistinguishable from 4 infrastructure files; diverges from every other library's own `include/<segment>/` convention; the inconsistency this ADR exists to fix | Rejected - the user explicitly asked for the reorg, and DD1 makes "do nothing" the one option that actively works against repo consistency |
| **B - Segment both `include/` and `src/`** (`include/smbios/tables/*.hpp` **and** `src/tables/*.cpp`) | Headers and sources colocate under the same segment name, arguably more intuitive at a glance | Diverges from `libsmbios_linux`'s own `src/` being flat; requires changing `file(GLOB)` into a two-directory (or recursive) glob instead of appending one more non-recursive line; invents a second convention where DD1 says there should be only one | Rejected - inconsistent with the one other multi-file library (`libsmbios_linux`) this repo already has to compare against |
| **C - One flat `include/smbios/*.hpp` namespace, with filename prefixes instead of a subdirectory** (e.g. `table_bios_information_entry.hpp`) | No new directory, no include-path churn beyond adding a prefix | Does not match any existing convention in this repo (no library uses filename prefixing instead of a subdirectory); `tables/` as a directory is strictly more discoverable (tab-completion, IDE tree view) than a shared filename prefix | Rejected - doesn't solve the sprawl problem, just renames it |
| **D - Recommended**: `include/smbios/tables/*.hpp`, flat `src/*.cpp`, unchanged factory | Matches `libsmbios_linux`'s exact precedent; one-line CMake glob change; zero factory edits; zero behavior change | Every existing entry header's `#include` site must be updated (11 files today, more as milestone 0010 lands); a maintainer scanning `src/` alone still can't tell entry-class `.cpp`s apart from infrastructure `.cpp`s by directory (only `include/` is segmented) | Adopted |

## Consequences

### Positive

- `libsmbios/include/smbios/tables/` becomes a single, discoverable
  location listing every implemented SMBIOS structure type - directly
  useful for milestone 0010's own "which types are done" tracking, and for
  any future contributor asking "is Type N implemented yet."
- Matches `libsmbios_linux`'s real precedent exactly; no new convention to
  document or remember.
- The self-registering factory (milestone 0004) absorbs the entire reorg
  with a zero-line change - a concrete payoff of that earlier design
  decision, not just an assertion.
- Milestone 0010's ~34 new entry classes land directly in their final
  location from their first commit, instead of being written flat and
  relocated a second time later (the reorg is deliberately Story 01.0, not
  a cleanup tacked onto the end).

### Negative

- Every one of the 11 existing entry headers' `#include` sites must be
  updated in the same change: `smbios_util/src/main.cpp` and
  `command_line_parser.cpp`, and every `tests/unit/*_entry_test.cpp` file
  that includes an entry header directly (confirmed necessary by grepping
  `#include <smbios/.*_entry.h>` across `smbios_util/` and `tests/`) - a
  real, if entirely mechanical, diff touching more files than the 11 moved
  headers themselves.
- `src/` staying flat while `include/` is segmented is a slightly
  asymmetric rule for a newcomer to internalize on first encounter (headers
  group, sources don't) - mitigated by it already being true, unremarked,
  of `libsmbios_linux` today, so it is at least a *consistent* asymmetry
  across the codebase, not a new one.
- `libsmbios/include/smbios/*.hpp`'s remaining 4 files (`smbios.hpp`,
  `abstract_smbios_entry.hpppp`, `smbios_entry_factory.hpp`,
  `smbios_entry_interface.hpp`) are themselves not segmented into any
  subdirectory of their own (e.g. a hypothetical `core/`) - left flat
  deliberately per Decision point 3, but a future reader could reasonably
  ask why `tables/` exists and `core/` doesn't; worth a one-line comment at
  the top of whichever of those 4 files is read first if this becomes a
  recurring question.

## Validation / Rollout

- Milestone 0010's Story 01.0 (first story, blocking every other story in
  that milestone) performs the move mechanically: `git mv` each of the 11
  existing header files, update their own `.cpp`'s `#include`, update every
  external `#include` site, update the one `CMakeLists.txt` glob line,
  rebuild, and run the full existing test suite (`ctest`) unmodified -
  DD3's "zero behavior change" bar is met only if every pre-existing test
  passes with no edits to its assertions, only to its `#include` lines
  where it includes an entry header directly.
- Every subsequent milestone-0010 story's new entry class is written
  directly against `include/smbios/tables/` from its first line - the
  reorg's success is also validated implicitly by *not* needing a second
  relocation pass once those land.

## Links

- **Roadmap story/task:** [docs/roadmap/0010-smbios-structure-coverage-expansion/01.0-tables-segment-reorg/README.md](/docs/roadmap/0010-smbios-structure-coverage-expansion/01.0-tables-segment-reorg/README.md)
- **Supporting specs:** [docs/roadmap/0010-smbios-structure-coverage-expansion/plan.md](/docs/roadmap/0010-smbios-structure-coverage-expansion/plan.md)
- **Diagrams:** _(none needed - a file-move, not a dependency-graph change)_

## Addendum (2026-11-09): `include/smbios/` itself renamed to `include/libsmbios/`

A follow-up, user-directed rename moved `libsmbios`'s entire public
`include/` tree from `libsmbios/include/smbios/` to
`libsmbios/include/libsmbios/` (headers now `#include <libsmbios/smbios.hpppp>`,
`#include <libsmbios/tables/*.hpp>`, etc.), at the same time renaming the
`smbios` CLI's own private header directory from `smbios/include/smbios_util/`
to `smbios/include/smbios/`. This addendum records it rather than rewriting
the Decision/Consequences sections above, which remain the accurate
historical record of the `tables/` segment decision itself - a separate,
later decision, not a correction of this one.

Rationale: every per-OS/legacy/interface library still places its headers
under `include/smbios/<segment>/*.hpp` (unchanged), all compiling into the
same `smbios::` namespace - but `libsmbios` is the one library whose
headers are the project's actual public API surface, not an internal
per-OS implementation detail. Giving it its own `include/libsmbios/` makes
that distinction visible at the `#include` line, where `#include <smbios/...>`
now unambiguously means "an internal per-OS/legacy header," never "the
core library's own public API."

No logic changed. Every `#include` site project-wide (library sources,
tests, the CLI) was updated mechanically along the same lines as the
original `tables/` move; `libsmbios/src/CMakeLists.txt`'s header glob and
`smbios/src/CMakeLists.txt`'s private-header glob were updated to match.
Full suite re-verified green after the rename (`ctest`, 4/4 targets).
