# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

C++ (C++20) utility that parses the SMBIOS/DMI firmware table and reports BIOS, motherboard, CPU, RAM, etc. Two CMake targets: `smbios` (static library, all system calls) and `smbios_util` (command-line front end, uses Boost.Program_options). Work in progress; Windows/Linux are the real targets. Mac has no native SMBIOS source (README asks for a UEFI implementation).

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
  - `tests/mock/` → `smbios_mock_test` - test doubles at a real injectable seam. Currently an intentionally empty placeholder (see the file's own header comment): no such seam exists yet pre-milestone-0005 (`ISmbiosSource`).
  - `tests/perf/` → `smbios_perf_test` - times real-hardware SMBIOS enumeration.
  - `tests/integration/` → `smbios_integration_test` - constructs a real `SMBios` against the live machine.
  - The latter two read the real machine's SMBIOS, so they typically need root/admin and fail cleanly (not crash) without it. Run one unit case with `./smbios_unit_test --run_test=SmbiosUnitTests/SMBiosVersionGreaterMajorWinsOverMinorTestCase --log_level=message`, or the whole suite via `ctest --test-dir build/<preset> --output-on-failure`.
  - See `docs/dev/cpp_coding_standard.md`'s Testing section for the classification scheme itself.
- `-DGIT_TAG=ON` makes the root CMakeLists create/delete **git tags** (`ver.major.minor.<builddate><n>`) as a build-versioning side effect; it is OFF by default, so leave it off to avoid mutating the repo's tags. This is unaffected by presets - see above.
- Source lists use `file(GLOB ...)`, so re-run `cmake` after adding or removing files.
- Compiler flags are set per compiler in the root `CMakeLists.txt` (Clang uses `-stdlib=libc++`, GCC `-pthread -fPIC`, Windows `/EHsc`).

## Architecture

Platform selection is done by **compile-time `#if` on the header**, not by CMake: `smbios.cpp` includes `win_bios.h` on `_WIN32`, otherwise `unix_bios.h`. Both define a class named `SMBiosImpl` (PImpl held by `SMBios` as `native_impl_`), so the rest of the library is platform-neutral.

Data flow in `SMBios` (`smbios/src/smbios.cpp`):
1. `SMBiosImpl` tries the OS-native source (Linux: sysfs `/sys/firmware/dmi/tables`, then EFI; Windows: native firmware-table API).
2. If `smbios_read_success()` is false, `SMBios` falls back to mapping physical memory (`PhysicalMemory`, PImpl over `posix_physical_memory` / `win_physical_memory`) and scanning 0xF0000–0xFFFFF for the `_SM_` / `_SM3_` entry point (`SMBIOSEntryPoint32/64`, validated via `checksum32/64`), then mapping the structure table it points to.
3. `read_smbios_table()` walks the raw table into a cached `std::vector<DMIHeader>`; each `DMIHeader` points into the raw buffer (no copies), so entries are views whose lifetime is tied to the `SMBios` object. The class exposes bidirectional iterators over these headers.

Typed entries: `AbstractSMBiosEntry` subclasses (`BiosInformationEntry`, `PortConnectionEntry`, `MemoryDeviceEntry`) are built from a `DMIHeader` + `SMBiosVersion` by `SMBiosEntryFactory`, which maps the SMBIOS structure type (`SMBios::SMBiosHandler` enum) to a Boost.Factory generator. The factory returns `nullptr` for unregistered types. To support a new structure type: add an entry class, include it in `smbios_entry_factory.h`, and register it in the factory constructor. The version is passed in because SMBIOS fields appear in later spec revisions and the parser must only read fields valid for the table's version.

Known rough edges: `smbios_util/src/command_line_parser.cpp` and `main.cpp` hold the CLI. (The `SMBiosVersion` `operator>`/`operator<` lexicographic-comparison bug this section used to describe is fixed - see `tests/unit/unit_tests.cpp`'s `SMBiosVersionGreater*`/`SMBiosVersionLess*` cases.)
