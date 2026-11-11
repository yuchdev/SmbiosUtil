# Story 02.0 - Self-Hosted vcpkg Overlay/Registry Port for `smbios_c`

**Parent milestone:** [plan.md](../plan.md)
**Status:** ⬜ Not started
**Depends on:** none (technical) - see Note below

## Scope

Author the vcpkg port itself (`ports/smbios-c/{portfile.cmake,vcpkg.json}`)
and the self-hosted distribution mechanism a consumer points their own
`vcpkg-configuration.json` at - implementing
[docs/adr/0004-package-manager-distribution.md](/docs/adr/0004-package-manager-distribution.md)'s
Decision point 2, superseding
[docs/roadmap/0007-c-abi-shim-for-cross-language-interop/status.md](/docs/roadmap/0007-c-abi-shim-for-cross-language-interop/status.md)'s
Story 03.0 Task 02 deferral.

**Note on the Depends-on field above:** `smbios_c` is already
self-contained (it whole-archive-links `libsmbios` inside its own
`CMakeLists.txt`, milestone 0007), so this story has no technical
dependency on Story 01.0's `libsmbios` install rules - the portfile only
ever needs to build and install the `smbios_c` target. It is listed second
in the milestone purely by this roadmap's usual foundation-first
convention (see `plan.md`'s Dependency graph note), and can be implemented
concurrently with Story 01.0 if that is more convenient.

## Mechanism choice: self-hosted vcpkg **git registry**, not an overlay-ports directory

vcpkg supports two ways to add ports outside the central registry:

1. **Overlay ports** - a consumer passes `--overlay-ports=<local-path>` (or
   sets `VCPKG_OVERLAY_PORTS`) pointing at a locally-cloned directory of
   port folders, on every `vcpkg install` invocation.
2. **Custom (git) registry** - a consumer adds an entry to their own
   `vcpkg-configuration.json`'s `registries` array naming this
   repository, a pinned baseline commit, and the port name(s) it
   provides; `vcpkg install smbios-c` then works without any extra flag,
   using the registry's own `versions/baseline.json` for reproducibility.

This story adopts **the git registry**, not overlay ports, because:

- A registry entry is **declarative and committed** to the consumer's own
  `vcpkg-configuration.json` - visible in their repo, reviewed in their
  PRs, resolved automatically by `vcpkg install` with no extra flag to
  remember on every invocation (unlike overlay ports, which are an
  invocation-time flag/environment variable a CI script can silently
  drop).
- A registry has its **own pinned baseline** (a commit SHA), giving the
  same reproducibility guarantee this project's own root
  `vcpkg-configuration.json` already relies on for `microsoft/vcpkg` -
  overlay ports have no baseline concept at all; a consumer gets whatever
  is on disk at invocation time.
- It is **the same shape of config block** a consumer already has in
  their own `vcpkg-configuration.json` (for `microsoft/vcpkg` itself),
  so adding this project as a second registry is a small, familiar diff,
  not a new concept - directly matching ADR 0004 DD3's "this project's
  own tooling" framing.
- It gives this project **a natural place to bump `smbios-c`'s port
  version in lockstep with releases** (`versions/baseline.json` +
  per-version port files) - DD2's "iterate at this project's own commit
  cadence."

The cost (ADR 0004's own Negative consequence): a consumer must already
know this repository exists and add its registry entry before `vcpkg
install smbios-c` works at all - accepted, per the ADR, as the honest
discoverability trade-off of self-hosting rather than a central
submission.

## Tasks

| #  | Document                                                       | Status         | Blocks |
|----|-----------------------------------------------------------------|----------------|--------|
| 01 | [Portfile and port vcpkg.json](01-portfile-and-port-vcpkg-json.md) | ⬜ Not started | 02     |
| 02 | [Self-hosted git registry layout](02-self-hosted-git-registry-layout.md) | ⬜ Not started | 03     |
| 03 | [Tests](03-tests.md)                                            | ⬜ Not started | -      |

## Key constraints

- **Wraps, does not fork, `smbios_c/CMakeLists.txt`'s existing `install()`
  rules.** The portfile calls `vcpkg_cmake_configure()` +
  `vcpkg_cmake_install()` + `vcpkg_fixup_pkgconfig()` against this
  project's own CMake build unchanged - no parallel packaging logic
  maintained inside `portfile.cmake` itself (ADR 0004 DD4).
- **No central-registry submission.** Nothing in this story opens a PR
  against `microsoft/vcpkg`; the port lives entirely under this
  repository's own `ports/smbios-c/`.
- **No Conan artifact of any kind** (`conanfile.py`, a Conan remote) - out
  of scope for this milestone (ADR 0004 Decision point 3).
- **`libsmbios`'s own C++ surface is not packaged here.** The port
  installs `smbios_c` only, matching ADR 0004 Decision point 1 exactly -
  it must not grow into also exporting `libsmbios::libsmbios` (that is
  Story 01.0's job, and is explicitly not published via any port in this
  milestone).

## Files or directories created or modified by this story

- `ports/smbios-c/portfile.cmake` (new).
- `ports/smbios-c/vcpkg.json` (new, port-level manifest - distinct from
  root `vcpkg.json`, which stays this project's own *consumer* manifest,
  untouched).
- `versions/baseline.json`, `versions/s-/smbios-c.json` (new - vcpkg's
  own required registry layout for version resolution).
- Documentation (exact location decided during implementation, e.g. a new
  `docs/dev/vcpkg-port.md` or a section in `docs/cli-usage.md`'s sibling
  docs) showing the consumer-side `vcpkg-configuration.json` snippet from
  `plan.md`'s contract C2.
