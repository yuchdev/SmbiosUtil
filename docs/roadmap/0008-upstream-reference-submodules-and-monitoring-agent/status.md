# Milestone 0008 - Upstream Reference Fetch Script and Monitoring Agent - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                                  | Status         | Tests |
|-------|--------------------------------------------------------|----------------|-------|
| 01.0  | Upstream reference fetch script and license isolation  | ✅ Complete | guard fixtures + fresh-clone sim; CI guard green ×3 OS |
| 02.0  | Reference baseline review workflow                     | ✅ Complete (02 superseded) | dry-run no-op advance; ctest 7/7 (macOS) |
| 03.0  | Upstream change-monitoring agent                       | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started · ⛔ Superseded (task-level only; see Notes)

## Notes & decisions

- Added at the user's request, following directly from the symlink
  mis-assessment incident during milestone 0005's planning (the macOS
  reference was assumed empty because `find` doesn't follow symlinks by
  default - a real submodule would have shown up in `git status` as
  present, making that mistake harder to make).
- **2026-11-03, user decision on Windows:** no Windows submodule in this
  milestone. No canonical, actively-maintained "dmidecode for Windows"
  port is known to exist; inventing one or guessing a URL was explicitly
  rejected as an option. Windows's reference is Microsoft's own
  `GetSystemFirmwareTable`/WMI documentation (already the basis for
  `win_bios.hpp`), watched by Story 03.0's agent as a documentation page,
  not a git submodule. Revisit only if a credible port is identified later
  - see `plan.md`'s "Confirmed upstreams" section.
- *[Superseded 2026-11-09 by the no-submodules ruling below: these are
  now fetch-script clone URLs in `scripts/upstream_references.json`, not
  submodule URLs. The URLs themselves are unchanged.]* Confirmed submodule
  URLs (verified via `git remote -v` against the existing symlinked
  checkouts, not guessed):
  - `dmidecode_linux` → `https://github.com/mirror/dmidecode.git`
  - `dmidecode_macos` → `https://github.com/acidanthera/dmidecode.git`
- **License note carried forward from this discovery:** both upstreams
  are GPL-2.0-or-later. This is why Story 01.0 treats build-exclusion as a
  structural requirement (R2), not just a comment in CLAUDE.md.
- **2026-11-09, Phase R audit (implement-milestone cold start)** - gaps
  found against the as-built tree, with dispositions:
  - *[Superseded 2026-11-09: there is no "remove symlinks" step and no
    submodule pin any more. Task 01.0/01 now specifies a fetch script that
    warns on and skips existing symlinks. The `b70f6ee`/`334e53a` values
    survive as the manifest's seed `baseline_sha`, re-verified via
    `git ls-remote`.]* *Symlinks absent on the implementing checkout* - `c-versions/` does
    not exist at all here (the machine-local symlinks were never
    recreated after the repo moved). Task 01.0/01's "remove symlinks"
    step is n/a; the pins are still taken from the user's own working
    checkouts `~/Projects/dmidecode_{linux,macos}` (`b70f6ee`/`334e53a`,
    both on `origin/master`), as the spec directs.
  - *[Superseded 2026-11-09: the `/c-versions/` ignore rule is now
    load-bearing for GPL isolation and must stay. Only its comment
    changes, in Task 01.0/01.]* *`.gitignore` line `/c-versions/` blocks `git submodule add`* - not
    named in Task 01.0/01's Files list; folded into that task (remove
    the ignore rule).
  - *Task 01.0/02 guard pattern contradicts the tree* - the spec greps
    every `*.h`/`*.cpp`/`*.c` for the bare substring `c-versions`, but
    ~40 existing provenance comments in `libsmbios/`, `libsmbios_linux/`,
    `libsmbios_macos/`, `sys_interface/` (e.g. `// Cross-checked against
    c-versions/dmidecode_linux/dmidecode.c`) would fail it, contradicting
    its own "passes on the current clean tree" criterion. Resolved from
    plan.md R2's own wording (which says *`#include` directive*, not any
    mention): the guard flags `c-versions` in any non-comment line of
    `CMakeLists.txt`/`*.cmake`/`vcpkg.json`, and in `#include` lines of
    C/C++ sources. Comments citing the reference stay legitimate - a
    comment cannot link GPL code into a binary.
  - *Guard file list predates the `.h` → `.hpp` rename* - `*.hpp` added
    to the guard's source-file set.
- **2026-11-09, user ruling: no submodules (spec-only redesign; nothing
  had been implemented yet).** The user's words: "Instead of including
  submodules, simply include a script that clones HEAD, to avoid any
  problems with GPL. No submodules with viral licenses. Implement
  monitoring agent as planned."
  - **New design:**
    - No `.gitmodules` and no GPL code or text tracked in any form.
    - `c-versions/` stays git-ignored, and its `/c-versions/` rule is
      now load-bearing.
    - `scripts/fetch_upstream_references.py` makes full (non-shallow)
      clones of each upstream's default-branch HEAD and fast-forwards
      clones that already exist. It warns on and skips any symlink or
      non-git directory, and it never deletes anything.
    - The tracked `scripts/upstream_references.json` (`name`, `path`,
      `url`, `baseline_sha`) is the single source of truth for the URLs.
      Its seed `baseline_sha` values are the upstream HEADs verified via
      `git ls-remote` on 2026-11-09:
      - linux: `b70f6ee0f2e879deebc630da20d27d350407889e`
      - macOS: `334e53aae57632329adf11fe5e852749b038a793`
    - R3 becomes a human-reviewed `baseline_sha` advance procedure.
  - **Per-task dispositions:**
    - **01.0/01** superseded by the fetch-script spec. The file was
      renamed `01-add-confirmed-submodules.md` →
      `01-fetch-script-and-manifest.md` and rewritten.
    - **01.0/02** amended (extended). It keeps the build-reference check
      and adds `.gitmodules`-entry and `git ls-files c-versions` checks.
    - **01.0/03** consumed with the success criteria updated: fresh clone
      plus script, clean `git status`, no `.gitmodules`.
    - **02.0/01** amended. It is now the baseline-advance procedure in
      `docs/dev/upstream_reference_review.md` (replacing
      `submodule_bump_procedure.md`), and its re-check lists name current
      files (`unix_bios.cpp` no longer exists).
    - **02.0/02** superseded by 01.0/02, because there are no pins to
      drift from. The file is kept with a banner.
    - **02.0/03** consumed with the Task 02 dependency dropped.
    - **03.0/01** amended. The agent runs the fetch script, then
      `git log <baseline_sha>..HEAD`; it records the reviewed HEAD SHA and
      recommends, but never applies, a manifest baseline update.
    - **03.0/02** and **03.0/03** consumed with the wording updated.
  - **Renames:**
    - Story folders: `01.0-git-submodule-conversion-and-license-isolation`
      → `01.0-upstream-reference-fetch-script-and-license-isolation`, and
      `02.0-submodule-update-and-pinning-workflow` →
      `02.0-reference-baseline-review-workflow`.
    - The milestone folder is deliberately **not** renamed, because
      external docs link to it.
- **2026-11-09, user decision: watcher invocation is on-demand only.**
  The user wants no standing CI automation and no Claude-in-CI
  credentials; the agent runs only when someone asks. Task 03.0/02's
  open choice is resolved accordingly, and the scheduled/cron/
  `workflow_dispatch` option is removed.
- **2026-11-09, Task 01.0/02 as-built deviations (verify-task PASS):**
  the guard also scans `tests/` and the repo-root build files
  (`CMakeLists.txt`, `*.cmake`, `vcpkg.json`, `vcpkg-configuration.json`,
  `CMakePresets.json`), because the only `vcpkg.json` lives at the root, so
  the spec's `vcpkg.json` check needs it. It also flags a tracked submodule
  gitlink even without a `.gitmodules`. The optional local pre-commit
  hook was **declined by the user (2026-11-09)**: CI is the enforcement
  point, and no extra `.claude/settings.json` hook is registered.
- **2026-11-09, pre-existing Windows CI failure (out of scope; user
  decision: fix separately after this milestone).** On PR #6, the
  isolation-guard step is green on all six CI jobs, including Windows. The
  Windows `build` job is red because four `smbios_c_*` ctest cases
  (`smbios_c_example`, `smbios_c_open_close_test`,
  `smbios_c_entry_iteration_test`, `smbios_c_typed_getters_test`) hit the
  60s timeout. This is identical on `master@066244b` (run 42630026324),
  the commit that introduced that timeout. It is not caused by milestone
  0008 (no C++ changed). Story 01.0 Task 03's CI criterion is judged on
  the guard step.
- **2026-11-09, PR #6 review hardening (`/pr-review`: feature LGTM,
  security PASS, no CRITICAL/HIGH).** The MEDIUM/LOW findings were fixed
  in `6b3423a`:
  - git runs with hooks and fsmonitor disabled and only the https
    protocol allowed;
  - an origin URL that doesn't match the manifest is skipped;
  - a symlinked `c-versions/` is refused and clone targets must resolve
    inside it;
  - clones use `--no-recurse-submodules --`, and userinfo URLs are
    rejected;
  - the guard matches case-insensitively and also flags tracked symlinks
    into `c-versions/` and `#define` lines.

  **Accepted residual risks:** filter/merge drivers in a pre-existing,
  locally tampered clone's `.git/config`; CMake string-splitting
  obfuscation; copy-pasted GPL code (covered by code review). Separately,
  the workflow has no top-level `permissions: contents: read` (this
  predates the PR; noted, not changed here).

### Story 01.0 - Upstream reference fetch script and license isolation (✅ 2026-11-09)

**Delivered** (PR #6: `9794f39`, `d817ddd`, `89c4b79`, `6b3423a`, `c421d8d`):
- `scripts/upstream_references.json` is the tracked manifest (name, path,
  HTTPS url, `baseline_sha`). It holds metadata only.
- `scripts/fetch_upstream_references.py` makes full clones of each
  upstream into git-ignored `c-versions/`, or fast-forwards existing
  ones. It never deletes, resets, or checks out `baseline_sha`. Git runs
  hardened (no hooks/fsmonitor, https only); a mismatched origin, a
  symlinked `c-versions/`, or a target that resolves outside it is
  skipped.
- `scripts/check_cversions_isolation.py` is the R2 guard. It flags, case
  insensitively:
  - non-comment `c-versions` references in build files (the build-product
    dirs, `tests/`, and the root build files);
  - `#include`/`#define` lines in C/C++ sources that reference it;
  - any `.gitmodules` entry or submodule gitlink;
  - any tracked file under `c-versions/`;
  - tracked symlinks pointing into it.

  Provenance comments remain legitimate.
- `.github/workflows/ci.yml` runs the guard right after checkout in the
  `build` and `sanitizers` jobs.
- `CLAUDE.md` and `.gitignore` describe `c-versions/` as script-populated,
  untracked, and never part of the build.

**Tests / gate:**
- `task-verifier` PASS on Tasks 01 and 02.
- Guard fixtures: every violation class fails, comment-only mentions
  pass.
- Fresh-clone simulation: fetch exits 0, SHAs match `git ls-remote`,
  `git status` is clean, no `.gitmodules`.
- A booby-trapped clone probe confirms the hardening.
- `/pr-review`: feature LGTM, security PASS.
- CI runs 42643434393 and 42652498002: the guard is green on every job on
  all three OSes. The Windows `build` job is red only from the
  pre-existing `smbios_c_*` timeout (see the note above; deferred by the
  user to after this milestone).

**Reconciliation:** the original submodule specs are superseded per the
2026-11-09 ruling. The 01.0/01 file was rewritten in place as the
fetch-script spec. 01.0/02 is consumed, with an as-built superset (wider
scan scope, symlink/`#define`/gitlink checks). 01.0/03 is consumed. The
optional local hook was declined.

### Story 02.0 - Reference baseline review workflow (✅ 2026-11-09)

**Delivered:**
- `docs/dev/upstream_reference_review.md` (R3) covers:
  - what `baseline_sha` means: metadata for "last triaged", not a build
    pin;
  - when to advance it: only after an `upstream-watch-*` report has
    triaged the range;
  - how: a normal human-reviewed PR citing the report, never automated;
  - per-upstream re-check lists, with the portable
    `git grep -l "c-versions/dmidecode_<linux|macos>"` as the
    authoritative list (24 Linux / 4 macOS files today);
  - per-preset build/test commands using CI's unprivileged `-R` filter
    (Windows adds `-C Debug`).
- Pointers were added to `docs/README.md` and the `CLAUDE.md` `c-versions/`
  paragraph.

**Tests / gate:**
- `task-verifier` PASS.
- Dry run (single no-op advance of `dmidecode_linux`, 2026-11-09): every
  step passed, the manifest is unchanged, and ctest passed 7/7 on macOS.
- Feature review was REQUEST_CHANGES on Windows portability (`python3`,
  a quote-nested one-liner, glob `grep`, missing `-C Debug`). It was
  resolved and re-checked.
- The security review was skipped because the change is docs-only, with
  no code or secret surface.
- Note: the Windows `ctest` line will currently hit the pre-existing
  `smbios_c_*` timeout (deferred above).

**Reconciliation:** 02.0/01 was consumed. Its re-check list was rebuilt
from the live grep, because the spec's snapshot had drifted:
`linux_sysfs_source.cpp` was dropped, `system_boot_information_entry.*`
moved from the macOS list to the Linux list, and
`system_information_entry.hpp` was added. 02.0/02 was superseded by
01.0/02. 02.0/03 was consumed.
