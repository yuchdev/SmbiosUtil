JUnit fixtures for `test_ctest_report.py` (milestone 0014, Story 03.0 Task 02).

Shaped on real `ctest --output-junit` output from ctest 3.31: `status="run|fail|notrun|disabled"`,
`<failure message="Failed|Timeout"/>`, and `<skipped message="SKIP_RETURN_CODE=77|Required Files Missing"/>`.
Test names are synthetic, and the fixtures hold no firmware data.

- `one_fail.xml` and `all_pass.xml` carry hostile `system-out` text that must never reach stdout.
  `one_fail.xml` also has a test name that fails the allowlist and must show as `<test #3>`.
- `doctype.xml` (entity expansion) and `malformed.xml` (truncated) must give `counts unavailable`.
- `zero.xml` is what ctest writes when a selection matches no test (`--no-tests=error` exits 8).
