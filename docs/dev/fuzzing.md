# Fuzzing `read_smbios_table()`

Milestone 0006, Story 02.0, Task 02 (R4's fourth boundary-test category).
This is a **local, developer-only tool** - it is not built by default, not
part of any `ctest` category, and **not run in CI** by this milestone (the
milestone's own Non-goals section explicitly excludes continuous/scheduled
fuzzing infrastructure such as OSS-Fuzz). If you want the hermetic,
always-green boundary tests instead, see `tests/unit/malformed_table_test.cpp`
and `docs/dev/cpp_coding_standard.md`'s Testing section.

## What it exercises

`tests/fuzz/read_smbios_table_fuzzer.cpp` feeds arbitrary fuzzer-generated
bytes directly into `smbios::SMBios::read_smbios_table()` via the same
test-only seam the hermetic unit-test layer uses
(`smbios::SmbiosTableWalkTestAccess::parse_synthetic_table()`, declared in
`tests/unit/synthetic_table_builder.hpp`, friended by
`libsmbios/include/libsmbios/smbios.hpp`) - no `ISmbiosSource`, no real hardware
access, is ever involved.

To avoid reconstructing a real `smbios::SMBios` (which walks the real
`ISmbiosSource` chain in its constructor) on every one of libFuzzer's
thousands-to-millions of calls per run, the harness constructs **one**
function-local `static smbios::SMBios instance` the first time
`LLVMFuzzerTestOneInput` runs, then only overwrites that instance's internal
buffer/state via `parse_synthetic_table()` on every subsequent call. This is
safe because libFuzzer calls `LLVMFuzzerTestOneInput` repeatedly within a
single process.

## Requirements

- A Clang/LLVM toolchain. **libFuzzer is a Clang-only feature** - this
  project's `SMBIOS_BUILD_FUZZERS` CMake option fails configure with a clear
  `FATAL_ERROR` if `CMAKE_CXX_COMPILER_ID` is not `Clang`/`AppleClang`,
  rather than silently skipping the target or silently miscompiling it.
  Apple Clang (the default on macOS) works; so does mainline LLVM Clang on
  Linux.

## Build

The fuzzer target is off by default, so a plain `cmake --preset <os>` /
`cmake --build --preset <os>` never builds it. To build it, pass
`-DSMBIOS_BUILD_FUZZERS=ON` explicitly on top of a preset (same convention
CLAUDE.md already documents for `-DGIT_TAG=ON` - presets don't bake this in,
so it needs a fresh configure with the flag added):

```sh
cmake --preset macos -DSMBIOS_BUILD_FUZZERS=ON   # or linux; Windows has no Clang toolchain here
cmake --build --preset macos
```

(Or without presets: `cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake -DSMBIOS_BUILD_FUZZERS=ON && cmake --build .`.)

This produces a `read_smbios_table_fuzzer` executable (built with
`-fsanitize=fuzzer,address,undefined`) alongside the other binaries in
`libsmbios/bin/`.

To go back to the normal, default build (fuzzer target excluded), configure
again without `-DSMBIOS_BUILD_FUZZERS=ON` (or explicitly `=OFF`) - this does
not affect `smbios_unit_test`/`smbios_mock_test`/`smbios_perf_test`/
`smbios_integration_test` or the `smbios` CLI in any way; the fuzzer target
is additive only.

## Run

```sh
cd build/macos/libsmbios/bin   # wherever CMAKE_RUNTIME_OUTPUT_DIRECTORY put it
./read_smbios_table_fuzzer -max_total_time=60
```

A short run (minutes, not hours) is enough to prove the harness actually
exercises the code and surfaces no immediate crash - that is the bar this
task's own spec sets, not an exhaustive or continuous fuzzing campaign.
libFuzzer prints a summary (`Done <N> runs`) and exits 0 on a clean run; any
crash is written to a `crash-<hash>` file in the current directory together
with a stack trace on stderr, which you can replay with
`./read_smbios_table_fuzzer crash-<hash>` for a single deterministic
reproduction.

Useful extra flags:

- `-max_len=4096` - cap the size of generated inputs (SMBIOS tables are
  small in practice; this keeps the corpus realistic without changing what
  the harness tests).
- `-jobs=N -workers=N` - parallel fuzzing processes.
- A corpus directory (`./read_smbios_table_fuzzer corpus/ -max_total_time=60`)
  to seed and accumulate interesting inputs across runs.

## Scope and limitations

- **Not wired into CI.** This milestone's plan.md explicitly lists
  continuous/scheduled fuzzing infrastructure as a Non-goal; `status.md`
  records the specific local run performed when this task landed.
- **Clang-only.** No GCC/MSVC fallback is provided or planned - libFuzzer
  itself does not support those toolchains.
- Only exercises the buffer-parsing path (`read_smbios_table()`/
  `count_smbios_structures()`/the string-walk inside it) - it does not
  exercise any `ISmbiosSource` (those read real firmware/OS state and are
  out of scope for a byte-buffer fuzzer).
