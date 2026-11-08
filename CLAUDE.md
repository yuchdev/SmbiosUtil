# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

C++ (C++20) utility that parses the SMBIOS/DMI firmware table and reports BIOS, motherboard, CPU, RAM, etc. Two top-level build products: `libsmbios` (static library, all system calls) and `smbios` (command-line front end, uses Boost.Program_options) - note the names: `libsmbios` is the core library, `smbios` is the CLI, not the other way around. Work in progress; Windows/Linux/macOS all have native sources as of milestone 0005: Linux reads `/sys/firmware/dmi/tables` then UEFI systab, macOS reads the `AppleSMBIOS` IORegistry entry via IOKit (no root/admin needed on either), Windows uses `GetSystemFirmwareTable()`; all three fall back to a legacy physical-memory scan.

`c-versions/` is untracked reference material: upstream `dmidecode` C sources for Linux and macOS. It is not part of the build; use it only as a reference for SMBIOS parsing behaviour.

## Build and test

Requires CMake and Boost >= 1.61 (static libs; `iostreams`, `program_options`, `unit_test_framework`, `date_time`). Dependencies are declared in the root `vcpkg.json` manifest (pinned to a `builtin-baseline` commit, also recorded in `vcpkg-configuration.json`) and resolved via the vcpkg toolchain file instead of hand-rolled `-D*_BOOST_DIR` flags.

Primary build path: `CMakePresets.json` at the repo root defines a hidden `default` configure preset (sets `binaryDir` to `build/<presetName>` and wires `CMAKE_TOOLCHAIN_FILE` to `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`) plus `windows`/`linux`/`macos` presets that inherit it with their respective generators (`Visual Studio 17 2022` / `Ninja` / `Ninja`), and matching build presets. With `VCPKG_ROOT` set in the environment:

```
cmake --preset linux        # or windows / macos
cmake --build --preset linux
```

`-DGIT_TAG=ON` is not baked into any preset's `cacheVariables`; pass it explicitly on top of a preset when needed, e.g. `cmake --preset linux -DGIT_TAG=ON`.

Fallback (no presets, e.g. an older CMake or a one-off toolchain path), out-of-source build:

```
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

This toolchain-file invocation replaces the old `-DWINDOWS_BOOST_DIR=<path>` / `-DMY_BOOST_DIR=<path>` / `-DBOOST_ROOT=<path>` flags on all platforms.

- Binaries are emitted to `smbios/bin/` (the CLI) and `libsmbios/bin/` (test binaries - `CMAKE_RUNTIME_OUTPUT_DIRECTORY` resolves there from each test target's own directory) relative to the build tree.
- Tests are Boost.Test executables under the top-level `tests/` tree, classified into four categories (`tests/<category>/`, each its own executable, each wired into `ctest` via `enable_testing()` in the root `CMakeLists.txt`):
  - `tests/unit/` → `smbios_unit_test` - deterministic, no hardware/privileges needed (hand-built `DMIHeader`/raw buffers, pure-function tests like `SMBiosVersion` comparison). Always expected to pass.
  - `tests/mock/` → `smbios_mock_test` - test doubles at a real injectable seam. The seam (`ISmbiosSource`) exists as of milestone 0005, but this suite is still an intentionally empty placeholder (see the file's own header comment) pending a concrete fake source to substitute through it.
  - `tests/perf/` → `smbios_perf_test` - times real-hardware SMBIOS enumeration.
  - `tests/integration/` → `smbios_integration_test` - constructs a real `SMBios` against the live machine.
  - The latter two read the real machine's SMBIOS, so they typically need root/admin and fail cleanly (not crash) without it. Run one unit case with `./smbios_unit_test --run_test=SmbiosUnitTests/SMBiosVersionGreaterMajorWinsOverMinorTestCase --log_level=message`, or the whole suite via `ctest --test-dir build/<preset> --output-on-failure`.
  - See `docs/dev/cpp_coding_standard.md`'s Testing section for the classification scheme itself.
- `-DGIT_TAG=ON` makes the root CMakeLists create/delete **git tags** (`ver.major.minor.<builddate><n>`) as a build-versioning side effect; it is OFF by default, so leave it off to avoid mutating the repo's tags. This is unaffected by presets - see above.
- Source lists use `file(GLOB ...)`, so re-run `cmake` after adding or removing files.
- Compiler flags are set per compiler in the root `CMakeLists.txt` (Clang uses `-stdlib=libc++`, GCC `-pthread -fPIC`, Windows `/EHsc`).

## Architecture

Every library lives in its own top-level, repo-root directory (not nested under `libsmbios/`) - `os_interface`, `libsmbios_win_legacy`, `libsmbios_posix_legacy`, `libsmbios_linux`, `libsmbios_macos`, `libsmbios_win`, plus `libsmbios` (core) and `smbios` (the CLI) - each shaped `<dir>/CMakeLists.txt`, `<dir>/include/smbios/<segment>/*.h`, `<dir>/src/*.cpp`. The `smbios::` C++ namespace and `#include <smbios/...>` convention is the same everywhere regardless of which repo-root directory a header physically lives in - that's the public API surface, not the build-target identity. Every one of these CMake target names already starts with `lib` (`libsmbios`, `libsmbios_win`, etc.), so each carries `set_target_properties(<target> PROPERTIES PREFIX "")` to stop CMake's automatic Unix "lib" prefix from doubling it into `liblibsmbios_win.a`.

Platform selection is a **CMake-time link choice**, not a compile-time `#if`: no `.cpp` in this codebase has a whole-file `#if defined(_WIN32)`/`__APPLE__`/`__linux__` guard any more - every one of those became redundant once its file moved into a library that root `CMakeLists.txt`'s own `if(WIN32)/elseif(APPLE)/elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")/else()` branch only ever builds for that one platform, so the guards were removed. (A few *non*-OS-selection conditionals remain and are correctly left alone: `win_native_api_helper.h`'s `#ifdef MIDL_PASS` is an IDL-compiler-mode guard, `win_system_information.h`'s `#ifdef _M_X64` is a compiler-arch macro, and the legacy POSIX `physical_memory.cpp`'s `#ifdef _SC_PAGESIZE` is a libc feature-detection fallback - none of these are about which OS to build for.) That root-level branch also selects which *legacy* flavor gets linked alongside each per-OS library:

| Platform | Per-OS library | Legacy flavor linked |
|----------|-----------------|------------------------|
| Windows | `libsmbios_win` | `libsmbios_win_legacy` |
| macOS | `libsmbios_macos` | `libsmbios_posix_legacy` |
| Linux | `libsmbios_linux` | `libsmbios_posix_legacy` |
| other `*nix` (BSD/Solaris) | *(none)* | `libsmbios_posix_legacy` |

`libsmbios` (core) always links `os_interface` (header-only) plus whichever legacy flavor and per-OS library the root branch selected. See `docs/adr/0001-per-os-smbios-source-libraries.md` for the library-boundary rationale, including its addenda on the repo-root relocation and the legacy split below.

**The legacy split**: `libsmbios_win_legacy` and `libsmbios_posix_legacy` are two *interchangeable* implementations of the exact same contract (unlike the per-OS libraries, which are not interchangeable with each other) - the legacy BIOS physical-memory scan. Each merges what used to be a `NativePhysicalMemory`/`PhysicalMemory` PImpl pair into one concrete `smbios::PhysicalMemory` class (the PImpl indirection existed only to hide the platform choice, which CMake's own library selection already does now), and each carries its **own duplicate copy** of `legacy_memory_scan_source.{h,cpp}` (`LegacyMemoryScanSource`, ~180 lines, 100% platform-neutral logic that only calls `PhysicalMemory`'s public interface) - a deliberate exception to this project's usual "don't duplicate shared logic" rule (ADR 0001's DD3), since the two flavors are never linked into the same build and a third library would exist only to host that one class. Both flavors expose the identical public header path, `smbios/legacy/{physical_memory.h,legacy_memory_scan_source.h}` - so every consumer's `#include <smbios/legacy/...>` resolves to whichever flavor got linked, with zero include-path change required anywhere.

Each platform implements `ISmbiosSource` (`os_interface/include/smbios/os_interface/ismbios_source.h`: `is_available()`, `try_read(out_table, major_version, minor_version)`, `source_name()`) one or more times, and that platform's own `make_default_source_chain()` definition (one per per-OS library - this is the actual link seam, not a single function with `#if` branches inside it) returns an ordered `vector<unique_ptr<ISmbiosSource>>`:
- **Linux**: `LinuxSysfsSource` (reads `/sys/firmware/dmi/tables/{smbios_entry_point,DMI}` directly, no privilege needed) → `LinuxEfiSource` (reads the physical address named by `/sys/firmware/efi/systab`'s `SMBIOS=`/`SMBIOS3=` line via `/dev/mem`) → `LegacyMemoryScanSource`.
- **macOS**: `MacOsUefiSource` (reads the `AppleSMBIOS` IORegistry entry via IOKit - `IOService:/AppleACPIPlatformExpert/bios/AppleSMBIOS`'s `"SMBIOS-EPS"`/`"SMBIOS"` CFData properties - no privilege needed, works on both Apple Silicon and Intel) → `LegacyMemoryScanSource`.
- **Windows**: `WindowsNativeSource` (wraps `win_bios.h`'s `SMBiosImpl`, `GetSystemFirmwareTable()`-based) → `LegacyMemoryScanSource`.

`SMBios`'s constructor (`libsmbios/src/smbios.cpp`) just walks the chain generically: `for (auto& source : make_default_source_chain()) { if (source->is_available() && source->try_read(table_buffer_, major_version_, minor_version_)) { source_name_ = source->source_name(); break; } }`, then calls `read_smbios_table()`. `LegacyMemoryScanSource` maps physical memory (`PhysicalMemory`) and scans 0xF0000-0xFFFFF for the `_SM_`/`_SM3_` entry point (`SMBIOSEntryPoint32/64`, validated via `checksum32/64`), then maps the structure table it points to. `source_name()` is surfaced by `SMBios::render_to_description()` for diagnostics (e.g. `"linux-sysfs"`, `"macos-uefi"`, `"legacy-scan"`).

`read_smbios_table()` walks the raw table into a cached `std::vector<DMIHeader>`; each `DMIHeader` points into the raw buffer (no copies), so entries are views whose lifetime is tied to the `SMBios` object. The class exposes bidirectional iterators over these headers.

Typed entries: `AbstractSMBiosEntry` subclasses are built from a `DMIHeader` + `SMBiosVersion` by `SMBiosEntryFactory::create()` (`libsmbios/include/smbios/smbios_entry_factory.h`), a stateless class backed by a module-scope registry populated at static-init time via the `REGISTER_SMBIOS_ENTRY(handler_value, class_name)` macro - each entry `.cpp` invokes this macro itself (never its header, or it would double-register when a test target includes the same header). The factory returns `nullptr` for unregistered types. To support a new structure type: add an entry class, invoke `REGISTER_SMBIOS_ENTRY` in its `.cpp`. The version is passed in because SMBIOS fields appear in later spec revisions and the parser must only read fields valid for the table's version.

Known rough edges: `smbios/src/command_line_parser.cpp` and `main.cpp` hold the CLI (that executable's own private header stays at `smbios/include/smbios_util/command_line_parser.h` - a leftover internal path name, harmless since it's never consumed outside this one target). (The `SMBiosVersion` `operator>`/`operator<` lexicographic-comparison bug this section used to describe is fixed - see `tests/unit/unit_tests.cpp`'s `SMBiosVersionGreater*`/`SMBiosVersionLess*` cases.)
