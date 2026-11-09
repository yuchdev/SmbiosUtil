# Upstream reference review: advancing `baseline_sha`

Milestone 0008, Story 02.0, Task 01. This is the one place the baseline-advance procedure is
written down. It tells a human how to move an upstream's `baseline_sha` in
`scripts/upstream_references.json` after a review of what changed upstream. The on-demand watcher
agent (`dmidecode-upstream-watcher`, defined in `.claude/agents/dmidecode-upstream-watcher.md`,
Story 03.0) may *recommend* an advance by following this document; it never applies one.

## Background

`c-versions/` holds local, git-ignored clones of two upstream `dmidecode` trees, which this
project consults as SMBIOS parsing references:

| Name | Path | Upstream URL |
|------|------|--------------|
| `dmidecode_linux` | `c-versions/dmidecode_linux` | `https://github.com/mirror/dmidecode.git` |
| `dmidecode_macos` | `c-versions/dmidecode_macos` | `https://github.com/acidanthera/dmidecode.git` |

Both are GPL-2.0-or-later. They are never tracked, built, or linked: there are no submodules, and
`scripts/check_cversions_isolation.py` fails CI if any build file, `#include`, `#define`,
`.gitmodules` entry, tracked path, or tracked symlink refers to them. Provenance *comments* in
source files that cite `c-versions/...` are fine.

`scripts/fetch_upstream_references.py` creates or fast-forwards the clones from the manifest. It
warns on and skips (never deletes or replaces) a symlink, a non-git directory, or a clone whose
`origin` URL differs from the manifest; it exits `1` only on a real failure (dirty tree, local
commits, detached HEAD, network or clone error, invalid manifest).

## What `baseline_sha` means

`baseline_sha` is the last upstream commit whose preceding range a watcher report has fully
triaged. It is metadata, not a pin: the checkout under `c-versions/` floats at upstream HEAD, and
nothing builds against `baseline_sha`. The range still to review is `<baseline_sha>..HEAD`.

## When to advance

Advance an upstream's `baseline_sha` only when all of these hold:

- A committed `docs/reviews/upstream-watch-<YYYY-MM-DD>.md` report (see [reviews/README.md](../reviews/README.md) for the
  report conventions) covers
  `<baseline_sha>..<new_sha>` for that upstream.
- The report labels every finding as *cosmetic*, *new information*, or *correctness*.
- The report states that the range was fully triaged.
- `<new_sha>` is the HEAD SHA the report recorded, never a later commit.

Never advance as a routine step for its own sake.

## How to advance

Commands use `<name>` for the upstream being advanced (`dmidecode_linux` or `dmidecode_macos`).
They use `python` as CLAUDE.md and the scripts' usage lines do; POSIX users may use `python3` if
`python` is absent (on Windows, `python3` is often the Microsoft Store stub).

1. Sync the checkouts and read the current HEAD of the upstream being advanced:

   ```bash
   python scripts/fetch_upstream_references.py
   git -C c-versions/<name> rev-parse HEAD
   ```

   The script prints `<name> <path> <HEAD SHA|skipped|failed>` per entry. A `skipped` or `failed`
   entry for the upstream you are advancing means the reference is not in a usable state; fix that
   first. (`--manifest <file>` reads an alternate manifest; entry paths still resolve against the
   repo root.)

2. Confirm the report's recorded SHA is reachable and is an ancestor of (or equal to) HEAD, and
   skim the range it covered. No output from the `merge-base` line means `<new_sha>` is not an
   ancestor of HEAD; stop and re-check the report:

   ```bash
   git -C c-versions/<name> cat-file -t <new_sha>
   git -C c-versions/<name> merge-base --is-ancestor <new_sha> HEAD && echo ok
   git -C c-versions/<name> log --oneline <baseline_sha>..<new_sha>
   ```

3. Edit that upstream's `baseline_sha` in `scripts/upstream_references.json` to `<new_sha>` (the
   full 40-character SHA, from the report). Change nothing else in the file.

4. Verify the edit:

   ```bash
   git diff --stat scripts/upstream_references.json
   python scripts/fetch_upstream_references.py
   python scripts/check_cversions_isolation.py
   ```

   The diff must touch exactly one line per advanced upstream. The fetch script validates the
   manifest and exits `1` on invalid input, so it must exit `0`. The isolation check must print
   `c-versions isolation check passed.`

5. Work through [What to re-check](#what-to-re-check-per-upstream) for that upstream.

6. Open a normal PR. Its description must include:
   - the report's path;
   - the old and new SHAs;
   - for each *new information* or *correctness* finding, a link to the follow-up issue or roadmap
     task, or an explicit "accepted, no action" note.

The advance is never automated, including when an agent proposed it.

## What to re-check, per upstream

The authoritative list of downstream files is whatever this returns at advance time. Re-run it and
trust it over the snapshot below if they disagree:

```bash
git grep -l "c-versions/dmidecode_linux" -- libsmbios libsmbios_win libsmbios_macos libsmbios_linux libsmbios_win_legacy libsmbios_posix_legacy sys_interface smbios smbios_c
git grep -l "c-versions/dmidecode_macos" -- libsmbios libsmbios_win libsmbios_macos libsmbios_linux libsmbios_win_legacy libsmbios_posix_legacy sys_interface smbios smbios_c
```

Snapshot taken when this document was written.

### `dmidecode_linux`

- Source layer:
  - `libsmbios_linux/include/smbios/linux/linux_sysfs_source.hpp` (sysfs
    `smbios_entry_point`/`DMI` handling);
  - `libsmbios_linux/src/linux_efi_source.cpp` and
    `libsmbios_linux/include/smbios/linux/linux_efi_source.hpp` (`/sys/firmware/efi/systab`
    `SMBIOS=`/`SMBIOS3=` parsing);
  - `sys_interface/include/smbios/sys_interface/smbios_version_fixup.hpp` (version-fixup quirks).
- Entry field tables in `libsmbios/src/`:
  - `bios_information_entry.cpp`
  - `baseboard_information_entry.cpp`
  - `system_enclosure_entry.cpp`
  - `processor_information_entry.cpp`
  - `cache_information_entry.cpp`
  - `system_slots_entry.cpp`
  - `physical_memory_array_entry.cpp`
  - `memory_device_entry.cpp`
  - `memory_array_mapped_address_entry.cpp`
  - `memory_device_mapped_address_entry.cpp`
  - `system_boot_information_entry.cpp`
- Matching headers in `libsmbios/include/libsmbios/tables/`: `baseboard_information_entry.hpp`,
  `cache_information_entry.hpp`, `physical_memory_array_entry.hpp`,
  `processor_information_entry.hpp`, `system_boot_information_entry.hpp`,
  `system_enclosure_entry.hpp`, `system_information_entry.hpp`, `system_power_supply_entry.hpp`,
  `system_slots_entry.hpp`.

### `dmidecode_macos`

- `libsmbios_macos/include/smbios/macos/macos_uefi_source.hpp` (IORegistry path `AppleSMBIOS`,
  `"SMBIOS-EPS"`/`"SMBIOS"` property names, and the Apple-Silicon-gating deviation recorded there;
  the implementation is `libsmbios_macos/src/macos_uefi_source.cpp`, which carries no citation of
  its own).
- `libsmbios_macos/src/macos_efi_reader.cpp` and
  `libsmbios_macos/include/smbios/macos/macos_efi_reader.hpp`.
- `libsmbios/include/libsmbios/tables/system_power_supply_entry.hpp`.

### Both upstreams

Build and run the unprivileged suites (the same `-R` filter CI uses; the integration and perf
suites read the real machine's SMBIOS and may need root/admin, so they are excluded). Linux and
macOS (single-config Ninja):

```bash
cmake --preset <preset>
cmake --build --preset <preset>
ctest --test-dir build/<preset> --output-on-failure --timeout 60 -R "smbios_unit_test|smbios_mock_test|smbios_c_"
```

Windows (multi-config Visual Studio generator, so `-C` is required; the build preset's
configuration is Debug in CI):

```bash
cmake --preset windows
cmake --build --preset windows
ctest --test-dir build/windows -C Debug --output-on-failure --timeout 60 -R "smbios_unit_test|smbios_mock_test|smbios_c_"
```

## Approval

An advance is a normal PR, reviewed like any other change. There is no exception for an advance
an agent recommended.

## Constraints

- **No GPL content.** Never copy upstream code or diffs into this repository, including PR
  descriptions and reports. Cite commit SHAs and file or function names only.
- **Single home.** The watcher agent links to this document rather than restating it.

## Dry run record

On 2026-11-09, before the watcher agent was built, the procedure was run once, as a single no-op
advance, for `dmidecode_linux` as a no-op
advance (`baseline_sha` to itself, `b70f6ee0f2e879deebc630da20d27d350407889e`, which equalled
upstream HEAD): steps 1 to 4 and the re-check command worked as written, and the manifest diff was
empty. The manifest edit itself was not committed.
