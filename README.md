# SMBIOS SDK

A cross-platform C++20 SDK that reads the SMBIOS/DMI firmware table and reports what it finds: BIOS, motherboard, CPU, memory, and 45 of the 49 structure types the SMBIOS 3.9.0 specification defines (the remaining 4 are either the table's own terminator or marked obsolete by the spec itself - see [SMBIOS structure coverage](#smbios-structure-coverage) below). It is more than a library and a CLI: the SDK ships a core library, a command-line tool, a C ABI for other languages, a desktop GUI browser, and (in development) packaging sources.

Windows, Linux, and macOS all have native ways to read the table - no root/admin needed on Windows or macOS; on Linux, sysfs's DMI table is typically root-only (mode 0400), so run as root there (see [CLI usage](docs/cli-usage.md#privileges)). Each falls back to a legacy BIOS physical-memory scan only if its native path is unavailable.

* **`libsmbios`** - the core C++ library. All system calls, all structure parsing, no UI.
* **`smbios`** - the command-line front end (`libsmbios`'s own namespace is `smbios::` everywhere; the CLI target happens to share that name).
* **`smbios_gui`** - an opt-in, read-only desktop browser (Dear ImGui) over the same structures, with a "Copy as..." action that reuses the CLI's output formatters. Build with `cmake --preset <linux|macos|windows>-gui`; see [CLAUDE.md](CLAUDE.md) and the [manual test plan](docs/test/manual-testing.md).
* **Packaging sources** *(in development)* - install rules, a self-hosted vcpkg port for `smbios_c`, GitHub Releases, and Rust/Python bindings over the C ABI; see roadmap milestones [0012](docs/roadmap/0012-package-manager-distribution/plan.md) and [0013](docs/roadmap/0013-language-bindings/plan.md).
* **`smbios_c`** - a C ABI shim over `libsmbios`, for calling into it from Python, Rust, C#, or any other language with C FFI support, without linking C++. See [`smbios_c/include/smbios/smbios.h`](smbios_c/include/smbios/smbios.h) and [CLAUDE.md](CLAUDE.md) for details, and `pkg-config --cflags --libs smbios_c` for build flags after installing.

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

No elevated privileges needed on Windows/macOS (Linux typically needs root). See [CLI usage](docs/cli-usage.md) for the full option list and sample output.

## Structured output

```bash
./build/<preset>/smbios/bin/smbios --format json    # text (default), xml, json, yaml, csv
```

Excerpt of the JSON shape, taken from the synthetic test fixture (`tests/data/output_formats/sample.json`), not from a real machine:

```json
[
  {
    "type": "BIOS Information",
    "fields": {
      "Vendor": "Example \"Firmware\" & Co.",
      "Version": "Example 1.0",
      "PCI is supported": true
    }
  }
]
```

See [Output formats](docs/output-formats.md) for every format.

## Setup guide

### Dependencies

Everything comes from vcpkg, declared in [`vcpkg.json`](vcpkg.json) and pinned to a `builtin-baseline` commit: `boost-iostreams`, `boost-program-options`, `boost-test`, `boost-date-time`. No manual Boost install or `-DBOOST_ROOT` flags needed.

### Finding `VCPKG_ROOT`

vcpkg has no standard install location: `VCPKG_ROOT` is simply the directory it was cloned or installed into (it contains the `vcpkg` executable, a `.vcpkg-root` marker file and `scripts/buildsystems/vcpkg.cmake`). To locate it on a machine, check in this order: the environment (`echo $VCPKG_ROOT`; GitHub Actions runners set `VCPKG_INSTALLATION_ROOT` instead); `vcpkg` on `PATH` (`dirname "$(realpath "$(command -v vcpkg)")"`, or `where vcpkg` on Windows); common clone locations (`~/vcpkg`, `C:\vcpkg`, `/usr/local/share/vcpkg`, `brew --prefix vcpkg`); the copy bundled with Visual Studio (`C:\Program Files\Microsoft Visual Studio\2022\<Edition>\VC\vcpkg`, already exported by a "Developer PowerShell for VS"); and, on Windows, `%LOCALAPPDATA%\vcpkg\vcpkg.path.txt` after `vcpkg integrate install`. Or let [`scripts/find_vcpkg_root.py`](scripts/find_vcpkg_root.py) do all of this and print what it finds:

```bash
python scripts/find_vcpkg_root.py
```

If there is none, clone one (a full clone, not `--depth 1`, because `vcpkg.json` pins a `builtin-baseline` commit): `git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh`.

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

Four C++ suites: `smbios_unit_test` (hermetic, synthetic-table-based, always green - this is where every one of the 45 implemented structure types gets its own coverage), `smbios_mock_test`, `smbios_perf_test`, and `smbios_integration_test` (the latter two read real hardware, so they need root/admin and degrade cleanly - not crash - without it), plus 5 individual `smbios_c_*` test executables under `tests/c_abi/` that exercise the C ABI shim (one of them, `smbios_c_compile_as_c_test`, is a plain C program - proof the public header genuinely compiles as C, not just C++ with `extern "C"`). Examples under `examples/` are also tested: the `smbios_example_*` CTest entries (skip them with `-DSMBIOS_BUILD_EXAMPLES=OFF`), and CI checks the doc samples with `scripts/check_doc_samples.py`. See [test subsystem](docs/test/test-subsystem.md) for the full breakdown.

## SMBIOS structure coverage

45 of 49 structure types are implemented (Types 0-46, 126; Types 5/6/10 are excluded as explicitly marked Obsolete by the spec itself, and Type 127/EndOfTable is the table's own terminator, never a decoded entry). Each one is a self-registering `AbstractSMBiosEntry` subclass under `libsmbios/include/libsmbios/tables/` - see the [architecture overview](docs/architecture/overview.md) for how a raw byte becomes a decoded structure, and that doc's own "how to add a new structure type" section if you want to add one of the 4 remaining types yourself.

## Documentation

- **[CLI usage](docs/cli-usage.md)** - options, sample output, what's not yet wired up.
- **[Architecture overview](docs/architecture/overview.md)** - the `ISmbiosSource` chain, the field-table accessor pattern, and how to add a new structure type.
- **[Output formats](docs/output-formats.md)** - the `--format` values, their layouts and escaping.
- **[Custom output formatter](docs/dev/custom-output-formatter.md)** - adding your own format via the formatter registry.
- **[Examples](examples/README.md)** - standalone programs that consume `libsmbios`.
- **[Test subsystem](docs/test/test-subsystem.md)** - what's tested, how, and why each category matters.
- **[Fuzzing](docs/dev/fuzzing.md)** - the local-only hardening harness for the table parser.
- **[C++ coding standard](docs/dev/cpp_coding_standard.md)** - style, Doxygen, and ownership conventions for contributors.
- **[Roadmap](docs/roadmap/README.md)** - the full history of how this project got here, milestone by milestone, and what's planned next.

Pull requests are welcome.
