# 02 - Dependency install robustness and vcpkg cache health

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01, Story 01.0 Task 02

## Objective

Make the two network-dependent setup paths fail fast, report which source they used in one line, and stay correct across image and compiler changes (plan E3, E7):

- the Linux apt install of GLFW/X11/autotools packages (inventory C15);
- the vcpkg binary cache (C03).

## Files

- **Create** `scripts/ci/apt_install.py`, which replaces the inline C15 step and keeps all of its behaviour:
  1. Read the package list from `scripts/ci/apt-packages-gui.txt` (new). It contains the 0011 final list - `libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config autoconf autoconf-archive automake libtool` - with a comment on where each group came from (glfw3 port message; `vcpkg_make` for `libx11`).
  2. Probe DNS for **every** host named in the active apt sources (`/etc/apt/sources.list.d/*.sources`, `/etc/apt/sources.list`, and `/etc/apt/apt-mirrors.txt` if present), not only `azure.archive.ubuntu.com`. If any host does not resolve, switch to the kernel.org Ubuntu mirror with the 0011 mechanism. Print one `info: apt mirror: <default|kernel.org> (<k> of <n> hosts unresolved: <names>)` line.
  3. Run `apt-get update` and `apt-get install` through `ci_step` with the 0011 options (`DEBIAN_FRONTEND=noninteractive`, `DPkg::Lock::Timeout=120`, `Acquire::Retries=1`, `Acquire::http(s)::Timeout=20`, `-qq --no-install-recommends`).
  4. Treat an `apt-get update` that exits 0 but leaves an empty package list for a required package's archive as a failure. This is the E3 silent "succeeded with nothing fetched" case. Check it with `apt-cache policy <first package>` showing a candidate.
  5. Print one `info:` line with the security pocket's origin. This is the 0011 follow-up: the fallback deliberately does not handle the security pocket, and the line makes that visible.
- **Create** `scripts/ci/vcpkg_cache_report.py`. After configure, it prints one line, `info: vcpkg cache <exact|partial|miss>, <n> archives, <MB> MB, <k> packages built from source`. It counts archives in the cache path before and after configure, and parses the configure log for vcpkg's own "installing from binary cache" / "building" lines tolerantly. If parsing fails, the line says `built-from-source unknown`.
- **Define** the cache key scheme (consumed by Story 03.0):
  - `key: vcpkg-<flavor>-<runner label>-<cxx id+major>-<hash(vcpkg.json, vcpkg-configuration.json, scripts/ci/requirements-tools.txt)>-<run_id>`
  - `restore-keys:` the same key without `-<run_id>`.
  - `<flavor>` is one of `default`, `asan`, `gui`.
  - A save happens only on pushes to `master` and on scheduled runs. Other runs restore only. This bounds cache churn under GitHub's per-repository cache quota.

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
- On a real runner (Story 03.0 Task 03): the `info: apt mirror:` line appears, and the GUI Linux leg builds.

## Success criteria

- [ ] C15's behaviour is preserved: same packages, same options, kernel.org fallback.
- [ ] The DNS check covers every configured source host.
- [ ] The silent "update fetched nothing" case is a hard failure with a one-line reason.
- [ ] Every job prints exactly one `info: vcpkg cache ...` line.
