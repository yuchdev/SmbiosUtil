# 0001 - Per-OS SMBIOS source libraries

> **Status:** Proposed
>
> **Date:** 2026-11-06
>
> **Supersedes:** _(none)_
>
> **Superseded by:** _(none)_

## Context

Milestone [0005](/docs/roadmap/0005-uefi-and-native-source-abstraction/plan.md)
already defines a runtime strategy interface, `ISmbiosSource` (contract
**C1**), and a per-platform factory, `make_default_source_chain()`
(contract **C2**), replacing `smbios.cpp`'s compile-time
`#if defined(_WIN32)` dispatch. As originally planned (Stories 01.0-04.0,
all still "Not started"), every `ISmbiosSource` implementation - Windows's
adapted `SMBiosImpl`, Linux's new sysfs/EFI sources, macOS's new IORegistry
source, and the extracted legacy physical-memory scan - would be a class
compiled straight into the single existing `smbios` static library,
selected at runtime by the chain but still physically one build artifact
per platform build.

The user explicitly asked for more than a class-level split: each
platform's implementation should be **packaged as its own library** (own
CMake target, own translation units), not merely a class inside `smbios`,
with the user's own working example being "3 for Windows, macOS, Linux, but
... if the architecture favors more ... 2 different for pre-UEFI and UEFI
Linux" - explicitly leaving the exact count open to whatever the real
dependency structure justifies, not mandating a specific number.

### Decision drivers

- **DD1 - Real reuse boundary, not a cosmetic one.** A library boundary
  only earns its complexity when some build configuration genuinely wants
  one piece without another. This project's own `CMakePresets.json`
  (`windows`/`linux`/`macos`) already picks exactly one OS per build - no
  configuration ever links more than one platform's native source.
- **DD2 - No new dependency cycle.** `SMBios` (core `smbios`) must call
  `make_default_source_chain()`, whose *definition* is necessarily
  platform-specific; every `ISmbiosSource` implementation must call shared
  decode primitives (`read_uN`, `detect_smbios_anchor()`) that live in
  core `smbios` today. If both directions point through the same `smbios`
  target, splitting sources into separate libraries creates a cycle
  (`smbios` → per-OS library → `smbios`) the moment `SMBios`'s constructor
  needs a symbol only the per-OS library defines.
- **DD3 - No duplicated logic across platforms.** The legacy
  physical-memory scan (`scan_physical_memory`/`checksum32`/`checksum64`/
  anchor detection, R3's last-resort source) is already,
  by construction, the *one* piece of this subsystem with zero
  platform-specific code of its own - its only OS dependency
  (`PhysicalMemory`/`NativePhysicalMemory`) is already hidden behind an
  existing PImpl + `#if`-split `.cpp` pair
  (`posix_physical_memory.cpp`/`win_physical_memory.cpp`). Packaging it
  three times (once per OS library) would either triple real object code
  for identical logic or require one OS library to depend on another's
  copy - neither is defensible once DD1 is applied honestly.
- **DD4 - Test coverage per library** (explicit user ask). Each library
  must be independently linkable by a Boost.Test executable without
  pulling in unrelated platform code, matching
  [docs/test/test-subsystem.md](/docs/test/test-subsystem.md)'s existing
  unit/mock/perf/integration split.

## Decision

Five CMake targets replace the single-library plan, not the user's literal
"3, maybe 5 for Linux" example - the real fault lines are **OS boundary**
(DD1) and **shared-vs-platform-specific code** (DD2/DD3), not "sysfs vs.
EFI":

| Target                     | Kind                        | Contents                                                                                   | Built when |
|-----------------------------|------------------------------|----------------------------------------------------------------------------------------------|------------|
| `smbios_source_interface`  | CMake `INTERFACE` (header-only) | `ISmbiosSource` (C1), `make_default_source_chain()` *declaration*, `fixup_smbios_version()` (R9), `read_u8/16/32/64`, `detect_smbios_anchor()` | always |
| `smbios_source_legacy`     | `STATIC`                     | `PhysicalMemory`/`NativePhysicalMemory` (moved verbatim, same internal `#if` split), `LegacyMemoryScanSource` | always |
| `smbios_source_windows`    | `STATIC`                     | `WindowsNativeSource` (adapted `SMBiosImpl`); defines `make_default_source_chain()` as `{WindowsNativeSource, LegacyMemoryScanSource}` | `WIN32` |
| `smbios_source_macos`      | `STATIC`                     | `MacOsUefiSource`; defines `make_default_source_chain()` as `{MacOsUefiSource, LegacyMemoryScanSource}` | `APPLE` |
| `smbios_source_linux`      | `STATIC`                     | `LinuxSysfsSource`, `LinuxEfiSource`; defines `make_default_source_chain()` as `{LinuxSysfsSource, LinuxEfiSource, LegacyMemoryScanSource}` | Linux |

`smbios` (core) links `smbios_source_interface` unconditionally, plus
exactly **one** of the three per-OS libraries, selected by a **CMake-time**
`if(WIN32) / elseif(APPLE) / elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")`
branch in `smbios/src/CMakeLists.txt` - this is the full resolution of
milestone 0005's R8: the branch moves from a C++ preprocessor `#if` to a
CMake `if()`, and shrinks to "which library to link," nothing else. Any
other `*nix` target (BSD/Solaris) links `smbios_source_interface` +
`smbios_source_legacy` directly and gets no per-OS library at all,
honestly reproducing today's stub-only behavior rather than inventing a
fourth named platform library nobody asked for.

`SMBios`'s constructor calls the free function
`smbios::make_default_source_chain()`, declared once in
`smbios_source_interface` and **defined exactly once**, in whichever
per-OS (or, for BSD/Solaris, a trivial inline fallback compiled into
`smbios` core itself) library CMake actually linked for this build - a
*link-seam*, not a preprocessor-seam. No build ever links more than one of
`smbios_source_windows`/`smbios_source_macos`/`smbios_source_linux`
together, so "cross-platform" here means source-tree/target-graph
organization per OS (matching this project's existing per-OS
`CMakePresets.json` entries), never one binary containing every
platform's code.

Linux's sysfs and EFI mechanisms (the user's own "2 for Linux" example)
stay in **one** `smbios_source_linux` library, not two: they are
`ISmbiosSource` implementations already independently selected at runtime
via the C2 chain, always built and always linked together on every Linux
configuration this project has (nothing ever wants one without the other -
DD1 fails for this particular split), and neither has the kind of
already-existing, genuinely-shared, OS-agnostic core that made splitting
out the legacy scan (DD3) worthwhile.

## Alternatives Considered

| Alternative | Pros | Cons | Reason rejected |
|-------------|------|------|-----------------|
| **A - Status quo**: classes inside one `smbios` library, runtime-selected chain, no new CMake targets (Stories 01.0-04.0 as originally scoped) | Smallest diff; no new build-graph complexity; already fully specced | Does not satisfy the user's explicit "package each platform as a separate library" ask; no independent-linkability for DD4's per-library test isolation | User-rejected scope; kept as the reason this ADR exists |
| **B - 3 libraries only** (Windows/macOS/Linux), no shared interface/legacy library - each OS library embeds its own copy of `PhysicalMemory`/`LegacyMemoryScanSource`/decode primitives | Matches the user's literal example count exactly; conceptually simple | Violates DD2 (still need `ISmbiosSource`/decode primitives shared somewhere acyclic) and DD3 (triples identical legacy-scan logic, or forces an arbitrary "one OS library depends on another's copy" rule) | Duplication with no corresponding benefit; the cycle problem resurfaces the moment any two OS libraries both need the same decode helper |
| **C - 5 libraries, mechanical split** along the milestone's existing 4 stories (windows, macos, linux-sysfs, linux-efi, +legacy or embedded) - the user's own example taken literally | Maps 1:1 onto existing story boundaries; "more libraries" literally satisfies the prompt's wording | Splits Linux sysfs/EFI despite DD1 finding no real consumer that wants one without the other; adds build-graph edges and two more CMakeLists.txt to maintain for zero architectural benefit | Rejected per DD1 - a library boundary with no corresponding "someone wants to link one without the other" case is complexity without payoff |
| **D - Recommended**: 3 per-OS libraries + 1 shared `smbios_source_legacy` + 1 shared header-only `smbios_source_interface` | Satisfies the user's "package per platform" ask; resolves DD2's cycle by construction (decode primitives sit below both core `smbios` and every per-OS library); resolves DD3 by sharing the one piece of logic that is genuinely OS-agnostic already; each library independently testable per DD4 | More CMake targets than a literal "3" reading of the user's example; requires moving `byte_reader.h`/`smbios_anchor.h` out of core `smbios`'s conceptual ownership | Adopted |

## Consequences

### Positive

- `smbios.cpp`'s `#if defined(_WIN32)` dispatch (R8) disappears from C++
  source entirely, replaced by one CMake `if()` block choosing a link
  target - a cleaner resolution than Story 01.0 Task 01's originally
  planned "minimal remaining `#if` in `smbios_source_chain.cpp`".
- The legacy physical-memory scan is compiled exactly once and reused by
  all three platforms' chains and by `LinuxEfiSource`'s `/dev/mem`
  mapping, removing a real (not hypothetical) triplication risk.
- Each new library is independently linkable by a focused Boost.Test
  executable (DD4), closing `tests/mock/`'s documented empty-placeholder
  gap at the same seam this ADR creates.

### Negative

- Five CMake targets (`smbios_source_interface`, `smbios_source_legacy`,
  `smbios_source_windows`, `smbios_source_macos`, `smbios_source_linux`)
  plus `smbios` itself is more build-graph surface than today's one
  library - more `CMakeLists.txt` files to keep in sync, and one more
  place (`smbios/src/CMakeLists.txt`'s OS-selection branch) that must be
  updated if a new platform is ever added.
- `byte_reader.h` and `smbios_anchor.h` move out of `smbios/include/smbios/`
  flat namespace into the new interface library's own include root - a
  real, if mechanical, header-relocation that every file including them
  today must update.
- The "link-seam" `make_default_source_chain()` definition (one per
  library) is a less familiar pattern than a single `#if`-guarded
  definition in one file; it needs a one-paragraph doc comment at the
  declaration site explaining why no definition exists in the header
  itself, or a future maintainer may reasonably wonder why the symbol
  looks unresolved when reading only `smbios_source_interface`.

## Validation / Rollout

- Story [01.1](/docs/roadmap/0005-uefi-and-native-source-abstraction/01.1-per-os-source-libraries/README.md)
  implements this ADR's target graph as a skeleton (library/CMake
  restructuring plus verbatim file relocation, zero logic change) between
  Story 01.0 (needs `ISmbiosSource` to exist first) and Stories
  02.0/03.0/04.0 (which then populate each per-OS library with real
  source logic from day one, instead of being refactored into this shape
  later).
- Rollout is verified the same way this milestone already verifies every
  other story: the existing functional/integration suite must pass
  unmodified on every platform once the skeleton lands, before any real
  new source is added on top of it.

## Links

- **Roadmap story/task:** [docs/roadmap/0005-uefi-and-native-source-abstraction/01.1-per-os-source-libraries/README.md](/docs/roadmap/0005-uefi-and-native-source-abstraction/01.1-per-os-source-libraries/README.md)
- **Supporting specs:** [docs/roadmap/0005-uefi-and-native-source-abstraction/plan.md](/docs/roadmap/0005-uefi-and-native-source-abstraction/plan.md) (contracts C1-C3)
- **Diagrams:** plan.md's own Mermaid dependency graph (no separate asset needed)
