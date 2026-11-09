# Milestone 0008 - Upstream Reference Fetch Script and Monitoring Agent - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                                  | Status         | Tests |
|-------|--------------------------------------------------------|----------------|-------|
| 01.0  | Upstream reference fetch script and license isolation  | 🔶 In progress (2/3 tasks) | - |
| 02.0  | Reference baseline review workflow                     | ⬜ Not started | -     |
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
