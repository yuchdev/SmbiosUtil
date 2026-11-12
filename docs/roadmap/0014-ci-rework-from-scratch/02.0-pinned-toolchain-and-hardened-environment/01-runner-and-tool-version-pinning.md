# 01 - Runner label and tool-version pinning; version header

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Every job runs on an explicit runner label with the same CMake/CTest/Ninja version on all three OSes and a vcpkg tool commit tied to the manifest baseline. Its first line of output says exactly what it ran on (RC4). This closes plan E2.

## Files

- **Create** `scripts/ci/requirements-tools.txt`: `cmake==<x.y.z>` and `ninja==<x.y.z>` from PyPI (the Kitware-maintained wheels), with `--hash=sha256:` lines for every wheel of the three runner platforms. Installed with `pip install --require-hashes --no-deps -r scripts/ci/requirements-tools.txt` into a job-local venv or `--user`. This needs no third-party action and pins by hash.
- **Create** `scripts/ci/print_env.py`. It prints the RC4 line, writes it as the first line of `$GITHUB_STEP_SUMMARY`, and sets `CI_ENV_LINE` in `$GITHUB_ENV` for the RC3 records. Each probe has a 10 s timeout, and a missing tool prints `-`, never a failure, because this step is `info`.
- **Record** in this task's as-built notes: the chosen labels, the CMake/Ninja versions, and the vcpkg commit.

## Functions and Classes

```python
def probe(argv: Sequence[str], pattern: str, timeout_s: float = 10.0) -> str: ...  # version or "-"
def env_line(job: str, leg: str) -> str: ...  # RC4, <= 240 chars
def main(argv: Sequence[str]) -> int: ...     # always 0
```

## Constraints

- **CMake version choice.**
  - Use one version for all legs.
  - It must be at least the root `cmake_minimum_required` and support the `CMakePresets.json` schema version.
  - It must support `ctest --output-junit` (3.21+) and `--no-tests=error`.
  - Prefer the newest 4.x patch available when the task runs. The 4.x summary wording is what the runners already produce on macOS/Windows.
- **Visual Studio generator.** The pip CMake must find the Windows image's VS 2022 instance through the `windows` preset's `Visual Studio 17 2022` generator. Verify this on the runner. Do not assume it.
- **vcpkg.** Set `run-vcpkg`'s `vcpkgGitCommitId` to the `builtin-baseline` in `vcpkg.json` (and `vcpkg-configuration.json`), or prove the action already derives it. The RC4 `vcpkg <sha12>` must equal the baseline's first 12 characters, and a mismatch is a hard failure of the cache step (Task 02).
- **Python** is the image's own `python3`/`python`. It is recorded, not pinned, because the scripts need only the stdlib, version 3.9 or later. `print_env.py` fails (hard) below 3.9.
- **Labels never use `-latest`.** This is enforced by Task 03's policy gate.

## Tests

- `scripts/ci/tests/test_print_env.py`: missing tools give `-`; the line is never longer than 240 characters; a hung probe (stub that sleeps) gives `-` within the timeout.
- First real run (Story 03.0 Task 01): the RC4 line on every leg shows the same `cmake`/`ctest`/`ninja` versions.

## Success criteria

- [ ] All legs report identical `cmake`, `ctest` and `ninja` versions in RC4.
- [ ] `vcpkg <sha12>` equals the manifest baseline on every leg.
- [ ] As-built notes list the labels and the reason for each, and the date of the `runner-images` list consulted.
