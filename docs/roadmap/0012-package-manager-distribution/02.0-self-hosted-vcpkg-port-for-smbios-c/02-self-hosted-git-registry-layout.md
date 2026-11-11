# 02 - Self-hosted git registry layout

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Lay out this repository as a vcpkg **custom git registry** (`versions/baseline.json`
+ per-port version files, per `README.md`'s Mechanism choice section)
and document the exact `vcpkg-configuration.json` snippet a consumer adds
to resolve `smbios-c` from it - implementing `plan.md`'s contract C2.

## Files

- **Create** `versions/baseline.json`:
  ```jsonc
  {
    "default": {
      "smbios-c": { "baseline": "1.0.0", "port-version": 0 }
    }
  }
  ```
- **Create** `versions/s-/smbios-c.json`:
  ```jsonc
  {
    "versions": [
      { "version": "1.0.0", "port-version": 0, "git-tree": "<computed by vcpkg x-add-version>" }
    ]
  }
  ```
  (`git-tree` is computed by vcpkg's own tooling, not hand-written -
  implementation runs `vcpkg x-add-version smbios-c` against a real
  commit to produce it.)
- **Create** documentation (exact file decided during implementation -
  candidate: `docs/dev/vcpkg-port.md`, linked from `docs/cli-usage.md`)
  showing the consumer-side registry entry, reusing `plan.md`'s contract
  C2 snippet verbatim, plus the procedure for bumping
  `versions/baseline.json` on a future `smbios_c` release (run `vcpkg
  x-add-version smbios-c` after bumping `ports/smbios-c/vcpkg.json`'s
  `"version"` and `SMBIOS_C_VERSION`, commit both together).

## Functions and Classes

N/A - packaging/registry metadata only.

## Constraints

- **The `versions/` directory lives at this repository's root** (vcpkg's
  own required registry layout - not configurable to a different path)
  - confirm this does not collide with any existing root-level
  directory or file before creating it.
- **The baseline bump procedure must be a documented, repeatable step**,
  not ad hoc - this is the concrete mechanism behind ADR 0004 DD2's
  "iterate at this project's own commit cadence" claim; if there is no
  written procedure, that claim is not actually true yet.
- Does not touch root `vcpkg.json` or `vcpkg-configuration.json` - those
  remain this project's own *consumer*-side manifest/config, unrelated to
  this repo's new role as a *registry* for `smbios-c`.

## Tests

- Covered by Task 03 - the real test is a consumer project's
  `vcpkg-configuration.json` (pointed at this repo as a registry)
  successfully resolving and installing `smbios-c` by name alone, with
  no `--overlay-ports` flag.

## Success criteria

- [ ] `versions/baseline.json` and `versions/s-/smbios-c.json` pass
      vcpkg's own registry-consistency check (`vcpkg x-add-version
      --dry-run` or equivalent, no CLI errors).
- [ ] The documented consumer-side `vcpkg-configuration.json` snippet is
      copy-pasteable and, combined with `"smbios-c"` in the consumer's
      own `vcpkg.json` dependencies, resolves without `--overlay-ports`.
- [ ] The baseline-bump procedure is written down somewhere a future
      release can follow without re-deriving it from first principles.
