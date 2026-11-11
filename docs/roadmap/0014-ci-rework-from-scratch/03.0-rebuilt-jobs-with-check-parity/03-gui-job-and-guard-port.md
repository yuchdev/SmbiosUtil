# 03 - `gui` job and the R-7/R-8 guard port

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 02

## Objective

Rebuild today's `gui-build` job as `gui` (C15-C20). Port the two merge-blocking proofs of [docs/security/2026-11-11-gui-test-tiers.md](/docs/security/2026-11-11-gui-test-tiers.md) from inline bash into a tested script, with **identical, fail-closed semantics**:

- R-7: no test-hook strings in the shipped `smbios_gui` (C18);
- R-8: the `gui-local` tier is absent on CI (C20).

## Files

- **Create** `scripts/ci/gui_guards.py`, with three subcommands. Each prints one `ok:` line, or RC1 discrepancy lines and exit 1:
  - `find-binary --build-dir build/<preset>`: the C17 search (`*/smbios_gui/bin/smbios_gui[.exe]`, regular file). Success: `ok: smbios_gui <path> (<bytes> bytes)`. None found: `smbios_gui binary: expected 1 -> actual 0`.
  - `hook-strings --binary <path>` (R-7): count the non-overlapping occurrences of the literal bytes `--exit-after-frames` and `usage: --exit-after-frames` in the binary. Read it in chunks with an overlap of `len(pattern)-1` bytes, so a match across a chunk boundary is counted once. Any count above 0 fails with `test hook present in shipped smbios_gui: expected 0 -> <n>` (today's message). An unreadable file fails closed, `hook string check could not read the shipped smbios_gui (<errno name>)`.
    - Semantic note for the threat-model review: `grep -c` counts *lines* that contain a match, while this counts *occurrences*. For the "must be 0" gate the two are equivalent: zero lines with a match if and only if zero occurrences.
  - `local-tier-absent --build-dir <dir> [--config Debug]` (R-8). All three checks are required:
    1. `ctest -N -L gui-local` must yield a `Total Tests: <n>` line with n = 0. A missing line fails closed (`'Total Tests:' line: expected present -> missing (exit <rc>)`). Parsing strips CR and tolerates leading whitespace.
    2. The same for `ctest -N -R "smbios_gui_local_test|smbios_gui_autoexit"`.
    3. A filesystem walk of the build dir (files **and directories**, the same as `find` without `-type f`) for names starting `smbios_gui_local_test` or `smbios_gui_autoexit` must find 0. On failure, print up to 5 relative paths. These are build-tree names, not data.
- **Modify** `ci-next.yml` - add the **`gui`** job (`needs: lint`, matrix `linux-gui`/`windows-gui`/`macos-gui` on the Story 02.0 labels):
  - Linux: `apt_install.py` (C15, `timeout-minutes: 10`);
  - vcpkg (cache flavor `gui`);
  - configure and build (`ci_step`, C16);
  - `find-binary` (C17) -> `hook-strings` (C18) -> `ctest_report.py gui_ci` (C19) -> `local-tier-absent` (C20).
  - No step runs `smbios_gui` (0011 Story 03.0 constraint, unchanged).

## Functions and Classes

```python
HOOK_PATTERNS: Final = (b"--exit-after-frames", b"usage: --exit-after-frames")
def count_occurrences(path: Path, pattern: bytes, chunk: int = 1 << 20) -> int: ...  # raises OSError
def ctest_total(argv: Sequence[str]) -> int | None: ...  # None = line missing -> caller fails closed
def local_tier_artifacts(build_dir: Path) -> list[Path]: ...
def main(argv: Sequence[str]) -> int: ...
```

## Constraints

- **Fail closed, everywhere.** Any `None`, any exception or any non-integer is a failure, never a 0. The 0011 rules carry over: "a missing `Total Tests:` line is a failure, not 0", and "`find` without `-type f`".
- The `security-auditor` reviews `gui_guards.py` against R-7/R-8 before merge. Code review blocks on any weakening.
- The `gui-ci` tier's own data hygiene is unchanged: its test output is fixture-free by design (0011 R-5), so `--output-on-failure` stays allowed for `gui_ci`.

## Tests

- `scripts/ci/tests/test_gui_guards.py`:
  - a fixture binary with 0, 1 and 3 occurrences of each pattern -> exit 0 / 1 / 1 with the exact count;
  - a pattern straddling a chunk boundary is counted once;
  - an unreadable file fails closed;
  - stub `ctest` printing `Total Tests: 0`, `Total Tests: 2`, nothing, and CRLF output -> pass / fail / fail-closed / pass;
  - a build tree containing only a `CMakeFiles/smbios_gui_autoexit.dir` **directory** -> fail.
- On runners: all three `gui` legs show `ok: smbios_gui ...`, `ok: no test hook strings ...`, `ok: tests gui_ci: ...` and `ok: no gui-local tests configured`, the same lines 0011's exit gate recorded.

## Success criteria

- [ ] C15-C20 present with identical gate semantics. The threat model review records PASS for the R-7/R-8 port.
- [ ] Each fail-closed branch has a test that proves it fails.
- [ ] No step executes `smbios_gui`.
