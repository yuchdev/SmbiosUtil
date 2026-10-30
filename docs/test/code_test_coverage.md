# Coverage Requirements Checklist

## Coverage Quick Start

This project requires **85% test coverage** on all code. This is enforced via a
coverage-instrumented CTest run.

### Running Tests with Coverage

```bash
# Configure a coverage build (one-time per build directory)
cmake -B build-coverage -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_EXE_LINKER_FLAGS="--coverage"

# Build and run tests
cmake --build build-coverage
ctest --test-dir build-coverage --output-on-failure

# Capture and render the report
lcov --capture --directory build-coverage --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' '*/_deps/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage-html
# Open: coverage-html/index.html
```

### What This Means

- ✅ Coverage is measured from an instrumented CTest run, not inferred
- ✅ CI **fails** if coverage drops below 85%
- ✅ Failed coverage checks block merging to main branches
- ✅ Coverage reports are always available in `coverage-html/`

### If Coverage Check Fails

1. **Identify uncovered code**: open `coverage-html/index.html`
    - Red lines = not covered by tests
    - Yellow lines = partially covered (branch not fully exercised)
    - Green lines = fully covered

2. **Add tests** for the uncovered code, or

3. **Mark intentional exclusions** with an `lcov` exclusion marker:
   ```cpp
   // LCOV_EXCL_START
   void unreachableDefensiveBranch() {
       // verified unreachable; guarded for defense-in-depth only
   }
   // LCOV_EXCL_STOP
   ```

4. **Re-run**:
   ```bash
   ctest --test-dir build-coverage --output-on-failure
   ```

### Threshold Details

- **Why 85%?** Balances quality with pragmatism
- **Not 100%**: Some code (defensive branches, platform-specific fallbacks) is
  hard/expensive to test
- **Not below 85%**: Ensures critical code paths are tested

### Configuration

Coverage build flags live in `CMakeLists.txt` (or a dedicated
`cmake/Coverage.cmake` module); the threshold is enforced in CI, not by a
local tool.

### Tips

- Run a single test binary: `ctest --test-dir build-coverage -R <TestSuiteName>`
- Faster local iteration: build without `--coverage` (CI still checks)
- Update the threshold: change the CI gate's configured minimum (don't lower
  it casually)

## Checklist Before Submitting a Pull Request

### ✅ Local Testing

- [ ] Configured and built the coverage build
  ```bash
  cmake -B build-coverage -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
  cmake --build build-coverage
  ```

- [ ] Ran tests locally with coverage
  ```bash
  ctest --test-dir build-coverage --output-on-failure
  lcov --capture --directory build-coverage --output-file coverage.info
  ```

- [ ] Coverage is ≥ 85%
  - [ ] `lcov --summary coverage.info` shows ≥ 85% line coverage
  - [ ] No failing test in the `ctest` run

### ✅ Code Review

- [ ] Reviewed uncovered code in `coverage-html/index.html`
  - [ ] Red lines (uncovered) are intentional or excluded with `LCOV_EXCL_*`
  - [ ] If uncovered, added an exclusion marker with a one-line reason
  - [ ] New code has test coverage ≥ 85%

- [ ] If coverage decreased:
  - [ ] Added new tests for new code paths
  - [ ] Documented why exclusions are needed
  - [ ] Consulted with team if the threshold is concerning

### ✅ Commit & Push

- [ ] Commit message references coverage status
  ```
  feat: add new feature with 87% coverage
  - Added 5 new test cases
  - Coverage improved from 82% to 87%
  ```

- [ ] Push to feature branch
  - [ ] CI pipeline runs successfully
  - [ ] All coverage checks pass
  - [ ] Coverage report available in CI artifacts

### ✅ Pull Request

- [ ] PR description includes:
  - [ ] Coverage before/after numbers
  - [ ] Any code intentionally excluded from coverage
  - [ ] Testing approach for new code

## Adding New Code

### For New Modules

1. **Create test file** first (TDD approach):
   ```
   src/libbios/feature/module.cpp
   tests/unit/feature/module_test.cpp   <- create this first
   ```

2. **Write tests** for all public functions
   ```cpp
   // tests/unit/feature/module_test.cpp
   TEST(ModuleTest, HandlesBasicCase) {
       auto result = module::feature(inputData);
       EXPECT_EQ(result, expectedOutput);
   }

   TEST(ModuleTest, HandlesEdgeCase) {
       // ...
   }
   ```

3. **Implement the module** to pass tests
4. **Register the target** in the relevant `CMakeLists.txt` (`add_executable`/
   `target_link_libraries` against `GTest::gtest_main`, plus `gtest_discover_tests`)
5. **Run coverage check** and verify new code is exercised

### For New Functions in Existing Modules

1. **Add a test case** to the relevant `*_test.cpp` file
2. **Verify the test fails** (red):
   ```bash
   ctest --test-dir build-coverage -R ModuleTest.NewFunction
   ```
3. **Implement the function** to pass the test
4. **Verify the test passes** (green)
5. **Check coverage** includes the new code

### Code Patterns to Test

Make sure you test:

- ✅ **Happy path**: normal, expected usage
  ```cpp
  TEST(FunctionTest, HappyPath) {
      EXPECT_EQ(function(validInput), expectedValue);
  }
  ```

- ✅ **Edge cases**: boundary conditions
  ```cpp
  TEST(FunctionTest, EmptyInput) {
      EXPECT_TRUE(function({}).has_value());
  }
  ```

- ✅ **Error handling**: invalid inputs
  ```cpp
  TEST(FunctionTest, InvalidInputThrows) {
      EXPECT_THROW(function(invalidInput), std::invalid_argument);
  }
  ```

- ✅ **Type/template variations**: different instantiations
  ```cpp
  TEST(FunctionTest, WorksWithString) { /* ... */ }
  TEST(FunctionTest, WorksWithInt) { /* ... */ }
  ```

## When Coverage Drops

### Step 1: Identify Coverage Gaps

```bash
lcov --capture --directory build-coverage --output-file coverage.info
genhtml coverage.info --output-directory coverage-html
open coverage-html/index.html
# Look for red lines (uncovered code)
```

### Step 2: Three Options

**Option A: Add Tests** (Preferred)
```cpp
TEST(ModuleTest, CoversMissingBranch) {
    // Test the uncovered line/branch
}
```

**Option B: Exclude with a marker** (Justified Cases)
```cpp
// LCOV_EXCL_START
void errorHandler() {
    // Hard to trigger deterministically in CI; verified manually.
}
// LCOV_EXCL_STOP
```

**Option C: Discuss with Team** (If Threshold Too High)
- Consensus needed to lower the threshold below 85%
- Document the rationale
- Update the CI gate's configured minimum

### Step 3: Re-verify

```bash
ctest --test-dir build-coverage --output-on-failure
lcov --summary coverage.info
# Should show >= 85% and PASS
```

## Troubleshooting

### "Coverage failed: 83% < 85%"

1. Check what's uncovered: `lcov --summary coverage.info` then open the HTML report.
2. Add tests or exclusion markers.
3. Re-run until ≥ 85%.

### "Some files show 0% coverage"

1. Verify the file was actually compiled with `--coverage` (a stale build
   directory silently skips instrumentation - reconfigure from scratch).
2. Ensure the test binary links against the object files that contain the
   function (a header-only function only shows coverage if a translation
   unit that includes it is exercised).
3. Confirm the `lcov --remove` filters above didn't accidentally exclude it.

### "HTML report not generated"

1. Confirm `lcov`/`genhtml` are installed (`brew install lcov` /
   `apt install lcov`).
2. Check `coverage-html/` was created by `genhtml`, not `lcov --capture` alone.
3. Re-run `genhtml coverage.info --output-directory coverage-html` explicitly.

## Summary

| Action                | Command                                                | Expected Result                |
|------------------------|---------------------------------------------------------|---------------------------------|
| Run all tests          | `ctest --test-dir build-coverage --output-on-failure`   | All pass + coverage captured    |
| Capture coverage       | `lcov --capture --directory build-coverage -o coverage.info` | `.info` file produced      |
| View coverage          | Open `coverage-html/index.html`                         | Colored per-file view           |
| Check specific test    | `ctest --test-dir build-coverage -R <SuiteName>`         | Test passes + coverage reported |
| Refresh coverage       | `lcov --zerocounters --directory build-coverage`         | Fresh calculation next run      |
| Find uncovered lines   | Look for **red** in HTML                                 | Know what to test next          |

---

**Remember**: Coverage ≥ 85% is required. Every PR must pass this check.

## Actual Test Layout

No test files were found under `tests/`.
