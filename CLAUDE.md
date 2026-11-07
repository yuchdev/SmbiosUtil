# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

C++ (C++20) utility that parses the SMBIOS/DMI firmware table and reports BIOS, motherboard, CPU, RAM, etc. Two CMake targets: `smbios` (static library, all system calls) and `smbios_util` (command-line front end, uses Boost.Program_options). Work in progress; Windows/Linux/macOS all have native sources as of milestone 0005: Linux reads `/sys/firmware/dmi/tables` then UEFI systab, macOS reads the `AppleSMBIOS` IORegistry entry via IOKit (no root/admin needed on either), Windows uses `GetSystemFirmwareTable()`; all three fall back to a legacy physical-memory scan.

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

- Binaries are emitted to `smbios/bin/` (library/CLI) or `smbios_util/bin/` (CLI) relative to the build tree; test binaries land in `smbios/bin/` too (`CMAKE_RUNTIME_OUTPUT_DIRECTORY` resolves there from each target's own directory).
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

Platform selection is a **CMake-time link choice**, not a compile-time `#if`: `smbios.cpp` has zero `#if defined(_WIN32)`/`__APPLE__`/`__linux__` branches. `smbios/src/CMakeLists.txt` links `smbios` against exactly one per-OS static library - `smbios_source_windows`, `smbios_source_macos`, or `smbios_source_linux` (selected by `if(WIN32)/elseif(APPLE)/elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")/else()`) - plus the always-linked `smbios_source_interface` (header-only) and `smbios_source_legacy` (the physical-memory-scan fallback, shared by every platform). See `docs/adr/0001-per-os-smbios-source-libraries.md` for the library-boundary rationale.

Each platform implements `ISmbiosSource` (`smbios/include/smbios/source_interface/ismbios_source.h`: `is_available()`, `try_read(out_table, major_version, minor_version)`, `source_name()`) one or more times, and that platform's own `make_default_source_chain()` definition (one per per-OS library - this is the actual link seam, not a single function with `#if` branches inside it) returns an ordered `vector<unique_ptr<ISmbiosSource>>`:
- **Linux**: `LinuxSysfsSource` (reads `/sys/firmware/dmi/tables/{smbios_entry_point,DMI}` directly, no privilege needed) → `LinuxEfiSource` (reads the physical address named by `/sys/firmware/efi/systab`'s `SMBIOS=`/`SMBIOS3=` line via `/dev/mem`) → `LegacyMemoryScanSource`.
- **macOS**: `MacOsUefiSource` (reads the `AppleSMBIOS` IORegistry entry via IOKit - `IOService:/AppleACPIPlatformExpert/bios/AppleSMBIOS`'s `"SMBIOS-EPS"`/`"SMBIOS"` CFData properties - no privilege needed, works on both Apple Silicon and Intel) → `LegacyMemoryScanSource`.
- **Windows**: `WindowsNativeSource` (wraps `win_bios.h`'s `SMBiosImpl`, `GetSystemFirmwareTable()`-based) → `LegacyMemoryScanSource`.

`SMBios`'s constructor (`smbios/src/smbios.cpp`) just walks the chain generically: `for (auto& source : make_default_source_chain()) { if (source->is_available() && source->try_read(table_buffer_, major_version_, minor_version_)) { source_name_ = source->source_name(); break; } }`, then calls `read_smbios_table()`. `LegacyMemoryScanSource` (shared fallback, used by every platform) maps physical memory (`PhysicalMemory`, PImpl over `posix_physical_memory`/`win_physical_memory`) and scans 0xF0000-0xFFFFF for the `_SM_`/`_SM3_` entry point (`SMBIOSEntryPoint32/64`, validated via `checksum32/64`), then maps the structure table it points to. `source_name()` is surfaced by `SMBios::render_to_description()` for diagnostics (e.g. `"linux-sysfs"`, `"macos-uefi"`, `"legacy-scan"`).

`read_smbios_table()` walks the raw table into a cached `std::vector<DMIHeader>`; each `DMIHeader` points into the raw buffer (no copies), so entries are views whose lifetime is tied to the `SMBios` object. The class exposes bidirectional iterators over these headers.

Typed entries: `AbstractSMBiosEntry` subclasses are built from a `DMIHeader` + `SMBiosVersion` by `SMBiosEntryFactory::create()` (`smbios/include/smbios/smbios_entry_factory.h`), a stateless class backed by a module-scope registry populated at static-init time via the `REGISTER_SMBIOS_ENTRY(handler_value, class_name)` macro - each entry `.cpp` invokes this macro itself (never its header, or it would double-register when a test target includes the same header). The factory returns `nullptr` for unregistered types. To support a new structure type: add an entry class, invoke `REGISTER_SMBIOS_ENTRY` in its `.cpp`. The version is passed in because SMBIOS fields appear in later spec revisions and the parser must only read fields valid for the table's version.

Known rough edges: `smbios_util/src/command_line_parser.cpp` and `main.cpp` hold the CLI. (The `SMBiosVersion` `operator>`/`operator<` lexicographic-comparison bug this section used to describe is fixed - see `tests/unit/unit_tests.cpp`'s `SMBiosVersionGreater*`/`SMBiosVersionLess*` cases.)
