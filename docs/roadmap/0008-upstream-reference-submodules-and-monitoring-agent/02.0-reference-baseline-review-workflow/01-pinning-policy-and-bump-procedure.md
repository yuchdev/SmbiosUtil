# 01 - Baseline-advance procedure

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Write the procedure a human follows to advance an upstream's
`baseline_sha` in `scripts/upstream_references.json`. A Story 03.0 agent
report may recommend the advance, but the agent never applies it.

## Files

- **Create** `docs/dev/upstream_reference_review.md`. The section names
  below are informal, but every one must appear.
  - **What `baseline_sha` means.** It is the last upstream commit whose
    preceding range a watcher report has fully triaged. It is metadata,
    not a pin: the local checkout under `c-versions/` floats at HEAD (see
    `scripts/fetch_upstream_references.py`). Nothing builds against it.
  - **When to advance.** Only when a committed
    `docs/reviews/upstream-watch-<YYYY-MM-DD>.md` report covers
    `<baseline_sha>..<new_sha>` for that upstream, labels every finding
    (R5), and states that the range was fully triaged. Never as a routine
    advance for its own sake, and never past the HEAD SHA the report
    recorded.
  - **How to advance.** Edit the upstream's `baseline_sha` to the
    report's recorded HEAD SHA, in a normal PR. The PR description must
    include:
    - the report's path;
    - the old and new SHAs;
    - for each *new information* or *correctness* finding, a link to the
      follow-up issue/roadmap task, or an explicit "accepted, no action"
      note.

    The advance is never automated. It goes through the same review as
    any other change, including when an agent proposed it.
  - **What to re-check, per upstream.** Each entry names real, current
    files:
    - `dmidecode_linux` range:
      - `libsmbios_linux/src/linux_sysfs_source.cpp` and
        `libsmbios_linux/include/smbios/linux/linux_sysfs_source.hpp`
        (sysfs `smbios_entry_point`/`DMI` handling);
      - `libsmbios_linux/src/linux_efi_source.cpp` and
        `libsmbios_linux/include/smbios/linux/linux_efi_source.hpp`
        (`/sys/firmware/efi/systab` `SMBIOS=`/`SMBIOS3=` parsing);
      - `sys_interface/include/smbios/sys_interface/smbios_version_fixup.hpp`
        (version-fixup quirks);
      - the field tables in `libsmbios/src/*_entry.cpp` that cite
        `c-versions/dmidecode_linux`, which today are:
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
      - the matching `libsmbios/include/libsmbios/tables/*_entry.hpp`
        headers.
    - `dmidecode_macos` range:
      - `libsmbios_macos/src/macos_uefi_source.cpp` and
        `libsmbios_macos/include/smbios/macos/macos_uefi_source.hpp`
        (IORegistry path `AppleSMBIOS`, `"SMBIOS-EPS"`/`"SMBIOS"`
        property names, and the Apple-Silicon-gating deviation already
        recorded there);
      - `libsmbios_macos/src/macos_efi_reader.cpp` and
        `libsmbios_macos/include/smbios/macos/macos_efi_reader.hpp`;
      - the macOS-cited entry tables
        `libsmbios/src/system_boot_information_entry.cpp`,
        `libsmbios/include/libsmbios/tables/system_boot_information_entry.hpp`,
        and `libsmbios/include/libsmbios/tables/system_power_supply_entry.hpp`.
    - **Both upstreams:** run `smbios_unit_test` (via `ctest`). The
      authoritative list of downstream files is whatever
      `grep -rn "c-versions/dmidecode_<name>"` over the build-product
      directories returns at advance time. The lists above are a snapshot
      and must be refreshed if that grep disagrees.
  - **Approval.** A normal PR, reviewed like any other change. There is
    no exception for an advance an agent recommended.

## Functions and Classes

N/A - documentation only.

## Constraints

- **Single home for the procedure.** It is written once here. Story
  03.0's agent definition links to it rather than duplicating it: the
  agent's job is to recommend an advance following this procedure, not to
  restate it.
- **No GPL content.** The procedure must never instruct anyone to copy
  upstream code or diffs into this repository, including PR descriptions
  and reports. A PR may cite commit SHAs and file/function names only.

## Tests

- **Dry run.** Follow the procedure once for one upstream, as a no-op
  advance from the seed `baseline_sha` to itself. Do this before Story
  03.0's agent is built to recommend advances, to confirm every step
  works as written.

## Success criteria

- [ ] The procedure's dry run succeeds exactly as written.
- [ ] Every "what to re-check" entry names specific existing files, not a
      vague "review the changes". `unix_bios.cpp` and other removed files
      are not mentioned.
