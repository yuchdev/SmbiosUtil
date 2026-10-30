# 02 - Fuzz harness

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Add a locally-runnable libFuzzer-style entry point feeding arbitrary bytes
into `read_smbios_table()`'s new test-only buffer-based entry point
(Story 01.0, Task 01), satisfying the fourth part of R4, without wiring
continuous/scheduled fuzzing infrastructure (explicit Non-goal).

## Files

- **Create** `smbios/test/fuzz/read_smbios_table_fuzzer.cpp`:
  ```cpp
  extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
      std::vector<uint8_t> buffer(data, data + size);
      smbios::read_headers_from_buffer(buffer); // Story 01.0 Task 01's new entry point; never throws/crashes on malformed input
      return 0;
  }
  ```
- **Modify** root `CMakeLists.txt`: a `SMBIOS_BUILD_FUZZERS` option,
  default `OFF`, building this target with `-fsanitize=fuzzer,address,undefined`
  only when explicitly requested (Clang-only - libFuzzer is a Clang/LLVM
  feature; document this limitation plainly rather than attempting a
  cross-compiler fuzz target).
- **Create** `docs/dev/fuzzing.md` (or a section in `docs/dev/building.md`):
  how to build and run this target locally (`cmake -DSMBIOS_BUILD_FUZZERS=ON
  ... && ./read_smbios_table_fuzzer -max_total_time=60`), and a note that
  it is not run in CI by this milestone.

## Functions and Classes

- `LLVMFuzzerTestOneInput` - new, standard libFuzzer entry point symbol.

## Constraints

- This target is never built by default and never runs in CI (Non-goal) -
  it is a developer tool, documented and present in the repo, not an
  automated gate.
- Depends on Story 01.0 Task 01's buffer-based test entry point existing.

## Tests

- Running the harness locally for at least a short duration (minutes, not
  hours) during this task's implementation, to prove it actually exercises
  the code and surfaces no immediate crash - the result (clean, or any
  finding investigated and fixed) is recorded in `status.md`, not treated
  as a permanent CI gate.

## Success criteria

- [ ] `cmake -DSMBIOS_BUILD_FUZZERS=ON` builds the fuzzer target on Clang.
- [ ] A local run of at least a few minutes surfaces no crash (or, if one
      is found, it is fixed and the fix is attributed to this task in
      `status.md`).
- [ ] `docs/dev/fuzzing.md` documents the local-only workflow clearly.
