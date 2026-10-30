# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

C++ (C++11/`gnu++1y`) utility that parses the SMBIOS/DMI firmware table and reports BIOS, motherboard, CPU, RAM, etc. Two CMake targets: `smbios` (static library, all system calls) and `smbios_util` (command-line front end, uses Boost.Program_options). Work in progress; Windows/Linux are the real targets. Mac has no native SMBIOS source (README asks for a UEFI implementation).

`c-versions/` is untracked reference material: upstream `dmidecode` C sources for Linux and macOS. It is not part of the build; use it only as a reference for SMBIOS parsing behaviour.

## Build and test

Requires CMake and Boost >= 1.61 (static libs; `iostreams`, `program_options`, `unit_test_framework`, `date_time`). Out-of-source build:

```
mkdir build && cd build
cmake ..            # on Windows pass -DWINDOWS_BOOST_DIR=<path>; elsewhere -DMY_BOOST_DIR / BOOST_ROOT as needed
cmake --build .
```

- Binaries are emitted to `bin/` relative to each target's source dir (`CMAKE_RUNTIME_OUTPUT_DIRECTORY` is `../bin` / `../../bin`).
- Tests are Boost.Test executables (`smbios_functional_test`, plus a performance test under `smbios/test/performance_test`). They read the real machine's SMBIOS, so they typically need root/admin. Run one case with `./smbios_functional_test --run_test=SmbiosFunctionalTests/SMBiosCreationTestCase --log_level=message`.
- `-DGIT_TAG=ON` makes the root CMakeLists create/delete **git tags** (`ver.major.minor.<builddate><n>`) as a build-versioning side effect; it is OFF by default, so leave it off to avoid mutating the repo's tags.
- Source lists use `file(GLOB ...)`, so re-run `cmake` after adding or removing files.
- Compiler flags are set per compiler in the root `CMakeLists.txt` (Clang uses `-stdlib=libc++`, GCC `-pthread -fPIC`, Windows `/EHsc`).

## Architecture

Platform selection is done by **compile-time `#if` on the header**, not by CMake: `smbios.cpp` includes `win_bios.h` on `_WIN32`, otherwise `unix_bios.h`. Both define a class named `SMBiosImpl` (PImpl held by `SMBios` as `native_impl_`), so the rest of the library is platform-neutral.

Data flow in `SMBios` (`smbios/src/smbios.cpp`):
1. `SMBiosImpl` tries the OS-native source (Linux: sysfs `/sys/firmware/dmi/tables`, then EFI; Windows: native firmware-table API).
2. If `smbios_read_success()` is false, `SMBios` falls back to mapping physical memory (`PhysicalMemory`, PImpl over `posix_physical_memory` / `win_physical_memory`) and scanning 0xF0000–0xFFFFF for the `_SM_` / `_SM3_` entry point (`SMBIOSEntryPoint32/64`, validated via `checksum32/64`), then mapping the structure table it points to.
3. `read_smbios_table()` walks the raw table into a cached `std::vector<DMIHeader>`; each `DMIHeader` points into the raw buffer (no copies), so entries are views whose lifetime is tied to the `SMBios` object. The class exposes bidirectional iterators over these headers.

Typed entries: `AbstractSMBiosEntry` subclasses (`BiosInformationEntry`, `PortConnectionEntry`, `MemoryDeviceEntry`) are built from a `DMIHeader` + `SMBiosVersion` by `SMBiosEntryFactory`, which maps the SMBIOS structure type (`SMBios::SMBiosHandler` enum) to a Boost.Factory generator. The factory returns `nullptr` for unregistered types. To support a new structure type: add an entry class, include it in `smbios_entry_factory.h`, and register it in the factory constructor. The version is passed in because SMBIOS fields appear in later spec revisions and the parser must only read fields valid for the table's version.

Known rough edges: the `SMBiosVersion` `operator>` / `operator<` in `smbios.cpp` compare major and minor independently (not lexicographically), so they are wrong for e.g. 3.0 vs 2.8. `smbios.cpp` still has a `// DEBUG` `<iostream>` include. `smbios_util/src/command_line_parser.cpp` and `main.cpp` hold the CLI.
