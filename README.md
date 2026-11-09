# SMBIOS SDK

A cross-platform C++20 library and command-line tool that reads the SMBIOS/DMI firmware table and reports what it finds: BIOS, motherboard, CPU, memory, and 45 of the 49 structure types the SMBIOS 3.9.0 specification defines (the remaining 4 are either the table's own terminator or marked obsolete by the spec itself - see [SMBIOS structure coverage](#smbios-structure-coverage) below).

Windows, Linux, and macOS all have native, unprivileged ways to read the table - no root/admin needed on any of the three. Each falls back to a legacy BIOS physical-memory scan only if its native path is unavailable.

* **`libsmbios`** - the core library. All system calls, all structure parsing, no UI.
* **`smbios`** - the command-line front end (`libsmbios`'s own namespace is `smbios::` everywhere; the CLI target happens to share that name).
* **`smbios_c`** - a C ABI shim over `libsmbios`, for calling into it from Python, Rust, C#, or any other language with C FFI support, without linking C++. See [`smbios_c/include/smbios/smbios.hpp`](smbios_c/include/smbios/smbios.hpp) and [CLAUDE.md](CLAUDE.md) for details, and `pkg-config --cflags --libs smbios_c` for build flags after installing.

## Quick start

Requires [vcpkg](https://github.com/microsoft/vcpkg) (set `VCPKG_ROOT`) and CMake 3.21+.

```bash
git clone https://github.com/yuchdev/SmbiosSdk.git
cd SmbiosSdk
cmake --preset <linux|macos|windows>
cmake --build --preset <linux|macos|windows>
```

Then run it:

```bash
./build/<preset>/smbios/bin/smbios
```

No elevated privileges needed. See [CLI usage](docs/cli-usage.md) for the full option list and sample output.

## Setup guide

### Dependencies

Everything comes from vcpkg, declared in [`vcpkg.json`](vcpkg.json) and pinned to a `builtin-baseline` commit: `boost-iostreams`, `boost-program-options`, `boost-test`, `boost-date-time`. No manual Boost install or `-DBOOST_ROOT` flags needed.

### Build presets

[`CMakePresets.json`](CMakePresets.json) defines one preset per platform (`windows`, `linux`, `macos`), each wiring `CMAKE_TOOLCHAIN_FILE` to vcpkg's toolchain file automatically:

```bash
export VCPKG_ROOT=/path/to/vcpkg     # or VCPKG_ROOT env var already set by vcpkg's own install
cmake --preset macos                  # or linux / windows
cmake --build --preset macos
```

Binaries land at `smbios/bin/smbios` (the CLI) and `libsmbios/bin/*` (test executables), relative to the build directory.

Fallback, if you're not using presets (an older CMake, or a one-off toolchain path):

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

### Running the tests

```bash
ctest --test-dir build/<preset> --output-on-failure
```

Four C++ suites: `smbios_unit_test` (hermetic, synthetic-table-based, always green - this is where every one of the 45 implemented structure types gets its own coverage), `smbios_mock_test`, `smbios_perf_test`, and `smbios_integration_test` (the latter two read real hardware, so they need root/admin and degrade cleanly - not crash - without it), plus 5 individual `smbios_c_*` test executables under `tests/c_abi/` that exercise the C ABI shim (one of them, `smbios_c_compile_as_c_test`, is a plain C program - proof the public header genuinely compiles as C, not just C++ with `extern "C"`). See [test subsystem](docs/test/test-subsystem.md) for the full breakdown.

## SMBIOS structure coverage

45 of 49 structure types are implemented (Types 0-46, 126; Types 5/6/10 are excluded as explicitly marked Obsolete by the spec itself, and Type 127/EndOfTable is the table's own terminator, never a decoded entry). Each one is a self-registering `AbstractSMBiosEntry` subclass under `libsmbios/include/libsmbios/tables/` - see the [architecture overview](docs/architecture/overview.md) for how a raw byte becomes a decoded structure, and that doc's own "how to add a new structure type" section if you want to add one of the 4 remaining types yourself.

## Documentation

- **[CLI usage](docs/cli-usage.md)** - options, sample output, what's not yet wired up.
- **[Architecture overview](docs/architecture/overview.md)** - the `ISmbiosSource` chain, the field-table accessor pattern, and how to add a new structure type.
- **[Test subsystem](docs/test/test-subsystem.md)** - what's tested, how, and why each category matters.
- **[Fuzzing](docs/dev/fuzzing.md)** - the local-only hardening harness for the table parser.
- **[C++ coding standard](docs/dev/cpp_coding_standard.md)** - style, Doxygen, and ownership conventions for contributors.
- **[Roadmap](docs/roadmap/README.md)** - the full history of how this project got here, milestone by milestone, and what's planned next.

Pull requests are welcome.
