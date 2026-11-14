# 02 - Dependency install robustness and vcpkg cache health

**Parent story:** [README.md](README.md)
**Status:** ✅ Complete (2026-11-13; on-runner checks pending Story 03.0, see As-built notes)
**Depends on:** Task 01, Task 04 (scrubbed child environment, `info()`), Story 01.0 Task 02

## Objective

Make the two network-dependent setup paths fail fast, report which source they used in one line, and stay correct across image and compiler changes (plan E3, E7):

- the Linux apt install of GLFW/X11/autotools packages (inventory C15);
- the vcpkg binary cache (C03).

## Files

- **Create** `scripts/ci/apt_install.py`, which replaces the inline C15 step and keeps all of its behaviour:
  1. Read the package list from `scripts/ci/apt-packages-gui.txt` (new). It contains the 0011 final list - `libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config autoconf autoconf-archive automake libtool` - with a comment on where each group came from (glfw3 port message; `vcpkg_make` for `libx11`).
  2. Probe DNS for **every** host named in the active apt sources (`/etc/apt/sources.list.d/*.sources`, `/etc/apt/sources.list`, and `/etc/apt/apt-mirrors.txt` if present), not only `azure.archive.ubuntu.com`. If any host does not resolve, switch to the kernel.org Ubuntu mirror with the 0011 mechanism. Print one `info: apt mirror: <default|kernel.org> (<k> of <n> hosts unresolved: <names>)` line. Each printed host name must match `^[A-Za-z0-9.-]{1,253}$`, else it is printed as `<invalid>` (threat model CR-4), and the line goes out through the reporter's `info()` (Task 04).
  3. Run `apt-get update` and `apt-get install` through `ci_step` with the 0011 options (`DEBIAN_FRONTEND=noninteractive`, `DPkg::Lock::Timeout=120`, `Acquire::Retries=1`, `Acquire::http(s)::Timeout=20`, `-qq --no-install-recommends`).
  4. Treat an `apt-get update` that exits 0 but leaves an empty package list for a required package's archive as a failure. This is the E3 silent "succeeded with nothing fetched" case. Check it with `apt-cache policy <first package>` showing a candidate.
  5. Print one `info:` line with the security pocket's origin. This is the 0011 follow-up: the fallback deliberately does not handle the security pocket, and the line makes that visible.
- **Create** `scripts/ci/bootstrap_vcpkg.py` (amended 2026-11-13 by the [CI-rework threat model](/docs/security/2026-11-13-ci-rework.md) CR-7 [OVERRIDES SPEC]; it replaces the `lukka/run-vcpkg` action, ADR 0005 P13). Behaviour, in order:
  1. Read `<sha>` from `vcpkg.json` `builtin-baseline` and from `vcpkg-configuration.json` `baseline`, and from the Task 01 pins file. All three must be the same 40-hex value; any mismatch or non-40-hex value is exit 1 with a discrepancy line `vcpkg baseline: expected <a> -> actual <b>`.
  2. In a fresh directory under `$CI_TEMP_DIR` (never the image's `VCPKG_INSTALLATION_ROOT`, never a `PATH` lookup of `vcpkg`): `git init`, `git fetch --depth 1 https://github.com/microsoft/vcpkg <sha>`, `git checkout --detach FETCH_HEAD`. The URL is a constant in the script, not an input.
  3. Integrity: `git rev-parse HEAD` must equal `<sha>`, else exit 1. Fetching by full commit id means the commit content is addressed by its hash, so no tag or branch can be repointed under the pin.
  4. Run `bootstrap-vcpkg.sh -disableMetrics` (`bootstrap-vcpkg.bat -disableMetrics` on Windows). The bootstrap downloads the prebuilt vcpkg tool binary named in `scripts/vcpkg-tool-metadata.txt` of that commit. During implementation, confirm (and record in the as-built notes) that the bootstrap verifies that binary against the SHA-512 in that file. If it does not, the script computes the SHA-512 of the downloaded `vcpkg[.exe]` itself with `hashlib` and fails on a mismatch.
  5. Export `VCPKG_ROOT=<bootstrapped dir>` and `VCPKG_DISABLE_METRICS=1` to later sub-steps of the same `stage.py` invocation through the process environment (not through `GITHUB_ENV`; P11). It also sets `VCPKG_BINARY_SOURCES` per the cache rules below.
  6. Print exactly one line, `ok: vcpkg bootstrap: <sha12>, tool <version>`, with `<version>` allowlisted as in Task 01.

  Every git and bootstrap call runs through `ci_step`, as an argv list, with no shell, and with the Task 04 scrubbed child environment.
- **Create** `scripts/ci/vcpkg_cache_report.py`. After configure, it prints one line, `info: vcpkg cache <exact|partial|miss>, <n> archives, <MB> MB, <k> packages built from source`. It counts archives in the cache path before and after configure, and parses the configure log for vcpkg's own "installing from binary cache" / "building" lines tolerantly. If parsing fails, the line says `built-from-source unknown`.
- **Define** the cache key scheme (consumed by Story 03.0):
  - `key: vcpkg-<flavor>-<runner label>-<cxx id+major>-<hash(vcpkg.json, vcpkg-configuration.json, scripts/ci/requirements-tools.txt)>-<run_id>`
  - `restore-keys:` the same key without `-<run_id>`.
  - `<flavor>` is one of `default`, `asan`, `gui`.
  - A save happens only on pushes to `master` and on scheduled runs. Other runs restore only. This bounds cache churn under GitHub's per-repository cache quota.
- **Cache poisoning mitigation** (amended 2026-11-13, threat model CR-2 [OVERRIDES SPEC]). The `files` binary-cache provider trusts an archive by its ABI-hash file name and does not authenticate its content, and `restore-keys` restores the newest matching entry. The rules:
  - **Cache writes come only from trusted refs:** a `push` to `refs/heads/master` or a `schedule` run on the default branch. Never from a tag run, a PR (same-repo or fork), a non-`master` branch push, or a dispatch. The save step is a separate `actions/cache/save` step guarded by those two conditions.
  - **Restore-only everywhere else.** PRs and forks restore through `actions/cache/restore`; GitHub already scopes their own entries to the PR ref, and they can read but never write the default-branch scope.
  - **The cache never feeds release artifacts.** On `CI_EVENT=tag`, packaging legs set `VCPKG_BINARY_SOURCES=clear` (a cold build from the pinned baseline, whose port downloads vcpkg checks against SHA-512), restore no cache and save no cache. Staging-tier runs on `master` may use the restored cache; their artifacts are disposable (ADR 0005 D9). The manifest records `"binary_cache": "none"|"restored"` per leg, and `publish` to a `release`-tier destination refuses `restored` (milestone 0012 Story 03.0/04.0). A future "verified cache" (for example signed archives) would need its own ADR before it may feed a release.
  - **Every job sets `VCPKG_BINARY_SOURCES` explicitly, starting with `clear;`** (`bootstrap_vcpkg.py` sets it), so a runner user cache (`~/.cache/vcpkg/archives`) or an inherited value is never consulted implicitly. Non-tag build legs use `clear;files,<cache dir>,readwrite` on save-eligible runs and `clear;files,<cache dir>,read` otherwise.
  - **`verify` and `publish` jobs restore no cache at all.** They need no compiler (ADR 0005 D3, clean runner). Task 03 adds the policy rule.
  - The runtime token that the cache service accepts is removed from every child process by Task 04's environment scrub, so a port build script or test cannot write a cache entry from a job that has no save step.

## Functions and Classes

```python
# apt_install.py
def apt_source_hosts() -> list[str]: ...
def unresolved(hosts: Sequence[str], timeout_s: float = 5.0) -> list[str]: ...
def use_fallback_mirror() -> None: ...       # writes the 0011 apt-mirrors.txt entry via sudo tee
def main(argv: Sequence[str]) -> int: ...

# vcpkg_cache_report.py
def count_archives(cache_dir: Path) -> tuple[int, int]: ...  # (files, bytes)
def main(argv: Sequence[str]) -> int: ...    # always 0 (info)

# bootstrap_vcpkg.py
VCPKG_URL: Final = "https://github.com/microsoft/vcpkg"
def pinned_baseline(repo_root: Path, pins_file: Path) -> str: ...   # 40-hex; raises on any disagreement
def checkout(sha: str, dest: Path) -> None: ...                     # init + fetch --depth 1 + detach; HEAD == sha
def bootstrap(dest: Path) -> Path: ...                              # returns the vcpkg binary; SHA-512 checked
def binary_sources(event: str, ref: str, packaging_leg: bool, cache_dir: Path) -> str: ...  # always starts "clear"
def main(argv: Sequence[str]) -> int: ...
```

## Constraints

- Step `timeout-minutes: 10` on the apt step (0011's value), and 5 on vcpkg setup. Story 02.0 Task 03 enforces them.
- `sudo` is used only for apt and for writing the mirror file, never for running the scripts themselves.
- The cache key includes the compiler identity, because the 26.04 move (E2) changed the compiler under an unchanged `ubuntu-latest` key. It deliberately excludes `ImageVersion`, which changes weekly and would make every week a cold build.
- No cache is written from a `pull_request` run. GitHub already scopes PR caches to the PR ref; this rule also stops PR churn.

## Tests

- `scripts/ci/tests/test_apt_install.py`, with stub `getent`/`apt-get`/`apt-cache` on a temporary `PATH`:
  - all hosts resolve -> default mirror;
  - one host fails -> kernel.org, and the line names it;
  - update succeeds but there is no candidate -> hard failure with an RC1 excerpt;
  - an install that stalls (stub sleeps) is killed by the runner's timeout, if one is set, else by the step budget. Assert that the RC1 failure still prints.
- `scripts/ci/tests/test_vcpkg_cache_report.py`: an empty or missing cache dir gives `miss`, and an unparseable log gives `unknown`.
- `scripts/ci/tests/test_bootstrap_vcpkg.py`, with a stub `git` and stub bootstrap on a temporary `PATH`:
  - a baseline mismatch between `vcpkg.json`, `vcpkg-configuration.json` and the pins file -> exit 1;
  - `git rev-parse HEAD` returning a different SHA -> exit 1;
  - a tool binary whose SHA-512 differs from the metadata file (where the script checks it) -> exit 1;
  - `VCPKG_ROOT` is the bootstrapped directory even when `VCPKG_INSTALLATION_ROOT` is set in the environment;
  - `binary_sources()`: `CI_EVENT=tag` on a packaging leg gives exactly `clear`; a PR gives `clear;files,<dir>,read`; a `master` push gives `...,readwrite`; every result starts with `clear`.
- `test_apt_install.py` additionally: a sources file naming host `::error::x` or `##teamcity[x]` prints `<invalid>`, and neither string appears in the output.
- On a real runner (Story 03.0 Task 03): the `info: apt mirror:` line appears, and the GUI Linux leg builds.

## Success criteria

- [x] C15's behaviour is preserved: same packages, same options, kernel.org fallback.
- [x] The DNS check covers every configured source host.
- [x] The silent "update fetched nothing" case is a hard failure with a one-line reason.
- [ ] Every job prints exactly one `info: vcpkg cache ...` line. (The script prints exactly one; calling it from every job is Story 03.0's wiring.)
- [ ] `bootstrap_vcpkg.py` is the only vcpkg source in every job; the RC4 `vcpkg <sha12>` equals the baseline on every leg.
- [ ] Cache writes happen only on `master` push and `schedule`; tag-event packaging legs show `cache miss` with `VCPKG_BINARY_SOURCES=clear`. (The rule is implemented and unit-tested in `binary_sources()`/`save_allowed()`; the save/restore step guards are Story 03.0's workflow.)

## As-built notes (2026-11-13)

**Files.** `scripts/ci/apt_install.py`, `scripts/ci/apt-packages-gui.txt`, `scripts/ci/bootstrap_vcpkg.py`, `scripts/ci/vcpkg_cache_report.py`; tests `scripts/ci/tests/test_apt_install.py` (12 cases), `test_bootstrap_vcpkg.py` (18), `test_vcpkg_cache_report.py` (12). Modified: `scripts/ci/ci_step.py` (see Decisions), `scripts/ci/pins.json` (Windows tool hashes), `scripts/ci/tests/test_ci_step.py` (2 time-limit cases). `python3 scripts/ci/run_tests.py`: `ok: scripts/ci tests: 105 tests` (61 existing + 44). The existing P11 scan in `test_ci_step.py` covers the new scripts. Stub-command tests skip on Windows (the stubs are `/bin/sh` trampolines, and `apt_install.py` is Linux-only).

**Bootstrap SHA-512 check (item 4), confirmed against the pinned commit's own sources.** `scripts/bootstrap.sh` (macOS/Linux) checks the downloaded tool with `vcpkgCheckEqualFileHash` against `VCPKG_MACOS_SHA`/`VCPKG_GLIBC_SHA`/`VCPKG_GLIBC_ARM64_SHA`/`VCPKG_MUSLC_SHA` in `scripts/vcpkg-tool-metadata.txt`. `scripts/bootstrap.ps1` (Windows) downloads `vcpkg.exe` with `tls12-download.exe` and checks **no** hash, and the metadata file has **no** Windows hash. So `bootstrap_vcpkg.py` hashes the tool with `hashlib` on every platform: against the metadata on macOS/Linux (defence in depth), and on Windows against new `pins.json` keys `vcpkg.tool_release_tag` (`2026-07-27`; must equal the metadata's `VCPKG_TOOL_RELEASE_TAG`, so a baseline bump forces a refresh) and `vcpkg.windows_tool_sha512.{x64,arm64}`. Those two hashes were computed on 2026-11-13 from the `vcpkg-tool` `2026-07-27` release assets, whose sha256 matched the digests GitHub's release API publishes; the same procedure reproduced `VCPKG_MACOS_SHA` exactly. A platform with no prebuilt tool fails closed.

**Decisions.**
- **`ci_step.run_step(timeout_s=...)`** (new, optional; the default keeps the old behaviour). The Tests section's "killed by the runner's timeout" needs a runner-side limit, which `run_step` did not have. On expiry: SIGTERM to the child's process group on POSIX (`sudo` relays it to `apt-get`, which the unprivileged script could not signal), SIGKILL after 5 s, exit `124`, first diagnostic `timed out after <t>s`, and the normal RC1 excerpt. `ci_step.log_path_for(label, log_dir)` was added too. Limits: apt update 180 s, `apt-cache policy` 30 s, install 360 s, DNS 5 s per host (inside the 10-minute step); each git call and the bootstrap 180 s (the 5-minute step budget is the outer bound).
- **Mirror file** is written with `sudo install -m 0644 <staged file> /etc/apt/apt-mirrors.txt` instead of 0011's `sudo tee`, because `ci_step` closes the child's stdin and `tee` would write an empty file. The content is unchanged (`https://mirrors.edge.kernel.org/ubuntu/<TAB>priority:1`).
- `DEBIAN_FRONTEND=noninteractive` is passed as `sudo`'s `VAR=value` argument, as in 0011, because `sudo` resets the environment.
- Hosts come from `sources.list.d/*.sources` (deb822 `URIs:`), `sources.list.d/*.list` and `sources.list` (one-line form; `*.list` files such as `microsoft-prod.list` are active sources although the spec did not name them) and `apt-mirrors.txt`. A host that fails the CR-4 allowlist is never passed to `getent`, counts as unresolved and prints as `<invalid>`. At most 5 names are printed, then `+<n> more`.
- No-candidate failure: one `error:`/annotation line `apt package lists failed (exit 1 ...): apt-get update exited 0 but <pkg> has no candidate`, the RC1 excerpt of the `apt-get update` log, an RC3 record, exit 1. The install does not run.
- Security line: `info: apt security pocket: <host> (direct; the mirror fallback does not cover it)`, `apt-mirrors.txt (follows the mirror choice)`, or `none configured`.
- `bootstrap_vcpkg.py`: sub-steps run through `ci_step` with a reporter proxy that suppresses their `ok:` lines, so success is exactly `ok: vcpkg bootstrap: <sha12>, tool <version> (<t>s)`. A failing sub-step prints its own RC1 output. The real tool prints `version 2026-07-27-<40-hex>` (51 characters, over Task 01's 40-character token cap), so `<version>` keeps the date plus 12 hex (`2026-07-27-98d7cb0cf1f4`). The exports go into `os.environ` only, so Story 03.0's `stage.py` must import the module and call `main()` in-process for later sub-steps to see them. `binary_sources()` rejects a cache path containing `,`, `;` or a backtick (vcpkg syntax). A tag run on a non-packaging leg restores read-only. The default cache directory is `$CI_TEMP_DIR/vcpkg-binary-cache`, created unless the sources are `clear`.
- `vcpkg_cache_report.py` has three subcommands: `snapshot` (before configure; silent; count kept in `$CI_TEMP_DIR/vcpkg-cache-before.json`, mode 0600, `O_NOFOLLOW`), `report --log` (the one `info:` line), and `key --flavor --runner-label [--cxx]` (key and restore prefix; `<hash16>` is the first 16 hex of SHA-256 over the three key files; `<cxx>` is `print_env.probe_cxx()`'s id plus major version, e.g. `gcc13`, `appleclang17`). The save rule `save_allowed(event, ref)` lives here and `bootstrap_vcpkg.py` uses it, so there is one copy. Classification: `miss` if the cache was empty before configure, or nothing was restored while something was built; `exact` if nothing was built; otherwise `partial` (also when the log is unparseable but the cache was not empty). Without a snapshot, the after-configure count stands in for "before". Usage and internal errors still print one `info: vcpkg cache report unavailable (...)` line and exit 0. Emitting the key as a step output needs Task 04's `Reporter.set_output`; until then it is an `info:` line.
- Task 04 (environment scrub, full `info()` contract) has not landed. These scripts use the existing `child_environment()` denylist and `Reporter.info`, and they will pick up Task 04's scrub through `ci_step` without changes.

**Locally verified (macOS host, 2026-11-13):** a real `bootstrap_vcpkg.py` run (real fetch of the pinned commit, real `bootstrap-vcpkg.sh`, SHA-512 check passed) printed `ok: vcpkg bootstrap: 00c5775211f4, tool 2026-07-27-98d7cb0cf1f4`, and `vcpkg_cache_report.py key` printed a well-formed key.

**Verified only on the CI runners (Story 03.0):**
- `apt_install.py` against the real `ubuntu-24.04` apt configuration: the hosts it finds, the `info: apt mirror:` and security-pocket lines, the kernel.org fallback when DNS fails, `sudo` relaying the time-out signal, and the GUI Linux leg building;
- `bootstrap_vcpkg.py` on Windows (`bootstrap-vcpkg.bat` started directly as an argv, and the pinned `vcpkg.exe` hash) and on Linux (glibc tool);
- the real configure log's vcpkg wording, i.e. that `report` prints a known `<k>` rather than `built-from-source unknown`, and the cache hit/miss line on a warm cache;
- the save/restore guards (they are `actions/cache/save`/`restore` conditions in Story 03.0's workflow), and `cache miss` with `VCPKG_BINARY_SOURCES=clear` on a tag-event packaging leg;
- the RC4 `vcpkg <sha12>` equal to the baseline on every leg.
