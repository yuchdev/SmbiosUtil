# 01 - Runner label and tool-version pinning; version header

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-13; on-runner checks pending Story 03.0 Task 01, see As-built notes)

## Objective

Every job runs on an explicit runner label with the same CMake/CTest/Ninja version on all three OSes and a vcpkg tool commit tied to the manifest baseline. Its first line of output says exactly what it ran on (RC4). This closes plan E2.

## Files

- **Create** `scripts/ci/requirements-tools.txt`: `cmake==<x.y.z>` and `ninja==<x.y.z>` from PyPI (the Kitware-maintained wheels), with `--hash=sha256:` lines for every wheel of the three runner platforms. Installed with `pip install --require-hashes --no-deps --only-binary :all: -r scripts/ci/requirements-tools.txt` into a job-local venv or `--user`. This needs no third-party action and pins by hash. `--only-binary :all:` is required (threat model CR-7): without it, a platform with no matching wheel falls back to an sdist build, which executes code.
- **Create** the pins data file (`scripts/ci/pins.json`; the name is fixed when this task runs and recorded in the as-built notes). It is the one file every stage reads (ADR 0005 D3) and lists: the runner labels, the CMake/Ninja versions, the vcpkg commit (which must equal `vcpkg.json` `builtin-baseline` and `vcpkg-configuration.json` `baseline`), and, for milestone 0012, the `aws` CLI version. `plan.py` (Story 04.0 Task 02) emits only labels found here, and policy rule P3 checks this file as well as the workflows (CR-6).
- **Create** `scripts/ci/print_env.py`. It prints the RC4 line and writes it as the first line of the job summary through the reporter backend (ADR 0005; `$CI_SUMMARY_FILE` on GitHub). It also hands `CI_ENV_LINE` to later steps for the RC3 records, by writing it to a file under `$CI_TEMP_DIR` rather than `$GITHUB_ENV`, so the hand-off stays platform-neutral. Each probe has a 10 s timeout, and a missing tool prints `-`, never a failure, because this step is `info`.
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
- **vcpkg.** The pinned vcpkg commit is the `builtin-baseline` in `vcpkg.json`, which must equal `baseline` in `vcpkg-configuration.json`. vcpkg is checked out and bootstrapped at that commit by `scripts/ci/bootstrap_vcpkg.py` (Task 02, amended 2026-11-13 by the [CI-rework threat model](/docs/security/2026-11-13-ci-rework.md) CR-7). No marketplace vcpkg action is used, and the image's own `VCPKG_INSTALLATION_ROOT` vcpkg is never used. The RC4 `vcpkg <sha12>` must equal the baseline's first 12 characters, and a mismatch is a hard failure of the bootstrap step (Task 02).
- **RC4 version tokens are allowlisted** (CR-4). `print_env.py` prints a probed version only if it matches `^[0-9A-Za-z.+_-]{1,40}$`, else `?`. The line goes out through the reporter's `info()` (Task 04), never a bare `print`. The `CI_ENV_LINE` hand-off file under `$CI_TEMP_DIR` is created mode 0600 with `O_NOFOLLOW` (CR-11, Task 04).
- **Python** is the image's own `python3`/`python`. It is recorded, not pinned, because the scripts need only the stdlib, version 3.9 or later. `print_env.py` fails (hard) below 3.9.
- **Labels never use `-latest`.** This is enforced by Task 03's policy gate.

## Tests

- `scripts/ci/tests/test_print_env.py`: missing tools give `-`; the line is never longer than 240 characters; a hung probe (stub that sleeps) gives `-` within the timeout; a stub tool whose version output is `::error::x` or `##teamcity[buildStatus text='x']` gives `?`, and neither string appears in the output.
- First real run (Story 03.0 Task 01): the RC4 line on every leg shows the same `cmake`/`ctest`/`ninja` versions.

## Success criteria

- [ ] All legs report identical `cmake`, `ctest` and `ninja` versions in RC4.
- [ ] `vcpkg <sha12>` equals the manifest baseline on every leg.
- [x] As-built notes list the labels and the reason for each, and the date of the `runner-images` list consulted.

## As-built notes (2026-11-13)

**Files.** `scripts/ci/requirements-tools.txt`, `scripts/ci/pins.json` (the pins data file's fixed name), `scripts/ci/print_env.py`, `scripts/ci/tests/test_print_env.py` (24 cases, picked up by `run_tests.py`'s `unittest discover`). `python3 scripts/ci/run_tests.py`: `ok: scripts/ci tests: 61 tests` (37 existing + 24). On a heavily loaded host (load average ~25) Story 01.0's `test_HugeLogBounded` once exceeded its 30 s bound (34.7 s; 17.7 s alone); it was left unchanged.

**Runner labels** (from the `actions/runner-images` README at commit `2cdd82f39871ec1e98584ee64008bc1261ce427f`, last changed 2026-11-03, consulted 2026-11-13; the same commit is recorded in `pins.json`):

| Leg | Label | Reason |
|-----|-------|--------|
| linux | `ubuntu-24.04` | x64; the label milestone 0011's apt fallback was verified on. `ubuntu-latest` now resolves to it, but `ubuntu-26.04` exists and `-latest` will move. |
| macos | `macos-15` | arm64 (macOS 15 Arm64 image), still GA. `macos-latest` already points to `macos-26`; moving to 26 is a deliberate pin change, not drift. |
| windows | `windows-2022` | **Not** the story README's candidate `windows-2025`. On the consulted list `windows-2025` (and `windows-latest`) is the `windows-2025-vs2026` image, whose readme lists only Visual Studio Enterprise 2026 (18.x), so the `windows` preset's `Visual Studio 17 2022` generator would not be found. `windows-2022` lists Visual Studio Enterprise 2022 17.14. Moving to VS 2026 needs a preset change and is out of scope. |

**Tool versions.** `cmake==4.4.4` (newest 4.x on PyPI on 2026-11-13; >= `cmake_minimum_required(VERSION 3.13)`, supports `CMakePresets.json` schema 6 (3.25+), `ctest --output-junit` (3.21+) and `--no-tests=error`), `ninja==1.13.2`. Hashes are for `manylinux2014_x86_64`, `macosx_10_10_universal2` (cmake) / `macosx_10_9_universal2` (ninja) and `win_amd64`; no sdist hash is listed, so `--only-binary :all:` plus `--require-hashes` cannot fall back to a build. The hosted images currently ship CMake 3.31.6 (Ubuntu 24.04, Windows 2022) and 4.4.5 (macOS 15) - exactly the E2/E4 split these pins remove. `aws_cli` `2.40.2` (newest `aws/aws-cli` 2.x tag on 2026-11-13) is recorded for milestone 0012; how it is installed and verified is 0012's, and RC4's fixed grammar has no `aws` slot, so `print_env.py` does not probe it.

**vcpkg commit.** `00c5775211f45cd08b37fce0484b4cb940e422ab` = `vcpkg.json` `builtin-baseline` = `vcpkg-configuration.json` `baseline` (checked by `PinsConsistencyTests`). RC4 prints `vcpkg 00c5775211f4` from the pins file; the hard mismatch check is `bootstrap_vcpkg.py` (Task 02).

**`print_env.py` decisions.**
- `cxx`: POSIX probes `c++ --version` (`appleclang`/`clang`/`gcc`). Windows probes `cl`; when no developer prompt put it on `PATH`, it reports the VS 2022 instance via `vswhere` (`vs2022 <installationVersion>`), which is what the VS generator uses.
- `cache` is `-` unless `--cache exact|partial|miss` is passed (RC4 is printed before any cache restore).
- `job`/`leg`/`image` (not tool output) are allowlisted too; `image=` folds whitespace to `/`, so `ImageOS ImageVersion` prints as `ubuntu24/20261101.360.1`.
- Hand-off file: `$CI_TEMP_DIR/ci-env-line.txt`, one line, `O_NOFOLLOW` + mode 0600 (an existing file is `fchmod`-ed to 0600; a symlink is refused on every OS). A write failure adds one `info:` line and still exits 0.
- The job summary is written only for a non-`plain` backend (plain's summary target is stdout, which already has the line).
- Output goes through `ci_step._emit` (the runner's single stdout writer), never a bare `print`.
- **Additive reporter change (ahead of Task 04):** a minimal `Reporter.info(line)` on all three backends plus `reporters.info_text()` (`safe_line`, first line only, cut to 240, an `info: ` prefix for a line starting `:`/`#`; TeamCity also `_neutralise`s). No existing event changed. Task 04 still owns the full `info()` contract and its tests, and may promote `_emit` to a public name.

**Verified only on the CI runners (Story 03.0 Task 01):**
- that pip CMake 4.4.4 finds the `windows-2022` image's VS 2022 instance through the `Visual Studio 17 2022` generator (not checked here);
- the success criteria above: identical `cmake`/`ctest`/`ninja` versions in RC4 on every leg, and `vcpkg <sha12>` equal to the baseline on every leg;
- the `vswhere` path and output on `windows-2022`, and `c++` resolving to GCC on `ubuntu-24.04`;
- the pip install of the cmake wheels on macOS and Windows (hashes from PyPI, not downloaded locally).
Locally verified (macOS host, 2026-11-13): `pip download --require-hashes --no-deps --only-binary :all: -r requirements-tools.txt` succeeds for `--platform manylinux2014_x86_64` (both wheels); for `macosx_15_0_arm64`, `macosx_13_0_x86_64` and `win_amd64` the same command was run for `ninja` only (it picks the hashed `universal2`/`win_amd64` wheel), because the 30-50 MB cmake wheels did not finish downloading over the local link - the cmake macOS/Windows hashes are PyPI's published sha256 for those exact filenames and are first exercised by the CI install step. `print_env.py` ran on macOS with all three backends.
