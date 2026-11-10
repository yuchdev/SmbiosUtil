# 0004 - Package manager distribution for `libsmbios` and `smbios_c`

> **Status:** Proposed
>
> **Date:** 2026-11-10
>
> **Supersedes:** _(none)_
>
> **Superseded by:** _(none)_

## Context

Today the only way for a downstream project to consume this repository's
C/C++ libraries is to vendor the source tree and build it with this
project's own `CMakePresets.json` + vcpkg-manifest toolchain
(`cmake --preset <os>` against `VCPKG_ROOT`). Root `vcpkg.json` already
*consumes* three Boost components as dependencies, pinned to a
`builtin-baseline` commit, but it is a manifest, not a port - nothing in
this repository lets an external project `vcpkg install` or
`conan install` this library the way it itself installs Boost.

Milestone 0007 (the C ABI shim, `smbios_c`) got the closest any target in
this tree has come to being packaging-ready:

- `smbios_c/CMakeLists.txt` is the project's **first and only** `install()`
  rule set (library, public headers, and a generated `smbios_c.pc` for
  `pkg-config` discovery), verified end-to-end against a trivial external C
  program built from `pkg-config --cflags --libs smbios_c` alone.
- `smbios_c` is deliberately `SHARED`, not a static archive, and whole-
  archive-links `libsmbios` plus an explicit export allowlist
  (`smbios_c.exported_symbols`/`smbios_c.version`) specifically so a
  consumer never needs to also link or understand `libsmbios` - see the
  CMakeLists.txt's own comments on why a static `smbios_c` would not hold
  that contract.
- Story 03.0 Task 02 explicitly deferred full vcpkg port publication
  (`portfile.cmake` + its own `ports/smbios-c/vcpkg.json`, registry
  submission, independent versioning) "to a future milestone," gated on
  "a real external consumer (the Python/Rust/C# binding this milestone's
  own motivation names) actually need[ing] `vcpkg install smbios-c` to
  work" (`docs/roadmap/0007-c-abi-shim-for-cross-language-interop/status.md`).
  This ADR is that revisit.

**`libsmbios` itself has no `install()` rule at all** (confirmed: neither
`libsmbios/CMakeLists.txt` nor `libsmbios/src/CMakeLists.txt` calls
`install()`). More importantly, `libsmbios` is not safely consumable as an
ordinary static-library dependency even if one were added today: every
concrete `AbstractSMBiosEntry` subclass registers itself with
`SMBiosEntryFactory` only via `REGISTER_SMBIOS_ENTRY`'s static initializer
in its own translation unit, and nothing in `libsmbios`'s own code names
those classes directly. The three places in this tree that link
`libsmbios` today - the `smbios` CLI (`smbios/src/CMakeLists.txt`), the
`smbios_c` shim (`smbios_c/CMakeLists.txt`), and the opt-in fuzzer target
(root `CMakeLists.txt`) - all carry the identical `-force_load`/
`/WHOLEARCHIVE`/`--whole-archive` link-option block, each with a comment
explaining that without it `SMBiosEntryFactory::create()` silently returns
`nullptr` for every entry type the caller doesn't name directly. A plain
`target_link_libraries(consumer PRIVATE libsmbios)` from an external
project would reproduce that exact silent-failure footgun, because nothing
in `libsmbios`'s build output currently forces the whole-archive link onto
consumers automatically (e.g. via an exported CMake package config that
sets it as an `INTERFACE` link option). `smbios_c` solves this problem
**once, internally** - that is precisely why it is `SHARED` and not
`STATIC` - so it is the one artifact in this repository that is honestly
ready to hand to a package manager today without also fixing a correctness
bug as a side effect.

### Decision drivers

- **DD1 - Don't package a footgun.** Any package-manager recipe this ADR
  recommends must not hand a downstream consumer an artifact that silently
  mis-decodes SMBIOS structures when linked the "obvious" way. This rules
  out packaging raw `libsmbios` today (see Context) without first doing
  real CMake-export work that is out of this ADR's scope.
- **DD2 - Minimize recipe/registry maintenance burden relative to this
  project's current size.** This is a single-maintainer-scale project with
  no existing release cadence or semantic-versioning policy for the C ABI
  (`plan.md`'s own Non-goals explicitly defer "ABI stability guarantees
  ... until the API surface ... has had real external users"). A
  packaging path that imposes its own CI matrix, review SLA, or recipe
  review queue before there is a real external consumer is premature
  overhead.
- **DD3 - Reach matched to the likely audience.** The stated motivation for
  `smbios_c` is Python/Rust/C#/Go FFI bindings, i.e. consumers who want a
  prebuilt shared library + headers + a discovery mechanism their own
  build tooling already understands (`pkg-config` today). C/C++ projects
  consuming `libsmbios` directly, if any emerge, are far more likely to
  already be CMake + vcpkg users than Conan users, mirroring this
  project's own toolchain (`CMakePresets.json`'s `VCPKG_ROOT`-driven
  toolchain-file pattern).
- **DD4 - Build on, don't duplicate, the `cmake --install` + `smbios_c.pc`
  path milestone 0007 already built and verified.** Every option below is
  evaluated by how thin a layer it adds on top of that existing,
  already-working mechanism, not by how much new packaging logic it
  requires maintaining in parallel.

## Decision

Package **`smbios_c` only**, as a **self-hosted vcpkg overlay/registry
port**, first - not `libsmbios`'s C++ surface, and not via Conan or a
central-registry submission, yet.

1. **What gets packaged:** `smbios_c` (the shared C ABI shim), because it
   is the one artifact that is both self-contained (DD1 - the whole-archive
   problem is solved inside its own `CMakeLists.txt`, not pushed onto the
   consumer) and already `install()`-able with a verified `pkg-config`
   story (DD4). `libsmbios`'s C++ surface is explicitly **out of scope**
   for package-manager distribution until a follow-up task adds (a) its
   own `install()` rules and (b) a CMake package config (`libsmbiosConfig.cmake`
   or an exported `libsmbios::libsmbios` target) that bakes the
   `-force_load`/`/WHOLEARCHIVE`/`--whole-archive` requirement in as an
   `INTERFACE` link option, so `find_package(libsmbios)` +
   `target_link_libraries(consumer PRIVATE libsmbios::libsmbios)` is
   correct by construction rather than correct only if the consumer
   independently discovers and copies this repo's own CMakeLists.txt
   comments. That is real design-and-implementation work this ADR
   deliberately does not resolve inline (see Links).
2. **Which manager first:** vcpkg, via a **self-hosted overlay port /
   custom registry** (option 1b below) - not a `microsoft/vcpkg` central
   submission, and not Conan. This repository already speaks vcpkg
   fluently as a *consumer* (`vcpkg.json`, `builtin-baseline`,
   `CMakePresets.json`'s toolchain-file wiring), so authoring a `portfile.cmake`
   that mostly just calls the existing `cmake --install` path this project
   already built and verified is a small, well-understood diff - a
   `vcpkg_cmake_configure()` + `vcpkg_cmake_install()` + `vcpkg_fixup_pkgconfig()`
   portfile, nothing novel. A self-hosted registry (a `vcpkg-configuration.json`
   `registries` entry pointing at this repo, or a sibling repo hosting
   just port files) lets the port iterate at this project's own commit
   cadence with no external review queue, matching DD2 - directly
   superseding Story 03.0 Task 02's deferral.
3. **Conan is deferred, not rejected**, until a real external consumer
   specifically asks for it (same trigger condition Story 03.0 Task 02
   already used for vcpkg, now applied one rung further out). Per DD3,
   this project's own tooling and its most plausible C/C++-direct
   consumers already lean CMake+vcpkg; a Conan recipe would duplicate the
   same whole-archive-link knowledge in `conanfile.py`'s `package_info()`
   with no existing consumer-side signal that it is needed yet.
4. **Central-registry submission (vcpkg's `microsoft/vcpkg`, or Conan
   Center)** is deferred past even the self-hosted port, for both managers,
   until the self-hosted port has at least one real external consumer and
   the C ABI's versioning policy (currently informal - see Non-goals in
   `docs/roadmap/0007-c-abi-shim-for-cross-language-interop/plan.md`) is
   settled enough to commit to the review/update cadence a central
   registry imposes.
5. **GitHub Releases of prebuilt artifacts** is adopted as a *parallel*,
   near-zero-maintenance baseline, not a substitute for the vcpkg port -
   see Alternatives and Consequences below for why it stays even after a
   package manager exists.

## Alternatives Considered

| Alternative | Pros | Cons | Reason rejected |
|-------------|------|------|-----------------|
| **1a - vcpkg, central `microsoft/vcpkg` port submission** | Maximum discoverability (`vcpkg install smbios-c` works with zero extra registry configuration for any vcpkg user); de facto quality bar signals project maturity | Requires passing `microsoft/vcpkg`'s own CI across all supported triplets and a community PR review queue with turnaround outside this project's control; every future `smbios_c` release needs a matching port-version bump PR reviewed by vcpkg maintainers, not just this repo's own maintainer (DD2) | Premature: no real external consumer yet to justify the review-cadence commitment; revisit once 1b has proven demand |
| **1b - vcpkg, self-hosted overlay/registry port** (adopted) | Iterates at this project's own cadence, no external review queue (DD2); thin diff on top of the already-working `cmake --install` + `smbios_c.pc` path (DD4); matches this project's own existing vcpkg-consumer tooling (DD3) | Less discoverable - a consumer must add this project's registry/overlay path to their own `vcpkg-configuration.json` before `vcpkg install smbios-c` works, rather than it working out of the box | Adopted - right-sized for current demand; a stepping stone to 1a, not a dead end |
| **2a - Conan Center submission** | Reaches Conan's user base (notably stronger than vcpkg's in some embedded/enterprise C++ shops); Conan 2.x's settings/options matrix is more expressive for cross-compilation scenarios | Same CI/review-queue burden as 1a, for a second registry, before any consumer has asked (DD2); `conanfile.py`'s `package_info()` would need to independently re-encode the whole-archive link requirement that 1b's portfile mostly inherits for free from this project's own CMake `install()` output | Rejected for now - no signal from likely consumers (DD3) that Conan reach is needed; revisit only if a real consumer requests it |
| **2b - Conan, self-hosted remote** | Same low-maintenance iteration benefit as 1b, for Conan users specifically | Doubles the packaging-metadata surface to maintain (both a vcpkg portfile and a `conanfile.py`) for an audience DD3 has no current evidence needs it | Rejected for now, for the same reason as 2a, one notch lower-cost - still not worth a second manager with zero demonstrated demand |
| **3 - GitHub Releases of prebuilt tarballs/zips + `cmake --install`** (adopted, in parallel) | Zero new recipe-review surface of any kind; directly reuses `cmake --install`'s existing output; serves consumers who want a binary without adopting *any* package manager (e.g. a CI job that just curls a tarball) | No dependency-graph integration (a consumer's own build system does not learn about `smbios_c`'s own dependencies automatically, unlike a vcpkg/Conan port); per-platform artifact matrix (Windows/Linux/macOS x compiler) is this project's own CI burden to produce and keep current each release | Adopted as a baseline alongside 1b, not instead of it - cheapest possible reach floor, but not a substitute for the dependency-graph benefits a real port gives a CMake+vcpkg consumer |
| **4 - System-package route (Homebrew formula / apt / distro packaging) for the `smbios` CLI** | Plausible for the CLI specifically - `smbios` is a `dmidecode`-shaped end-user tool, and that audience (someone who wants to run a command, not link a library) is well served by `brew install`/`apt install` with zero C++ toolchain involved | Solves a different problem than this ADR's scope (library *distribution* for downstream code, not CLI *installation* for end users); no package-manager-neutral story exists yet for even the CLI's own versioning | Out of scope for this ADR - worth its own future ADR if/when the `smbios` CLI (not `libsmbios`/`smbios_c`) is the thing being distributed to end users, not developers |

## Consequences

### Positive

- Reuses, rather than re-derives, milestone 0007's already-verified
  `cmake --install` + `smbios_c.pc` path (DD4) - the vcpkg portfile is a
  thin wrapper, not a second packaging implementation to keep in sync by
  hand.
- Packaging `smbios_c` first - and explicitly deferring `libsmbios`'s raw
  C++ surface - means this ADR cannot accidentally ship the whole-archive
  footgun described in Context/DD1 to an external consumer; it names the
  real prerequisite work (`libsmbios` `install()` rules + an
  `INTERFACE`-link-safe package config) as a named follow-up instead of
  quietly skipping it.
- A self-hosted vcpkg registry costs this project nothing in external
  review latency (DD2) and is a strict subset of the work a future central
  submission would need anyway (the portfile itself is reusable
  verbatim), so choosing 1b now does not foreclose 1a later.
- GitHub Releases artifacts (option 3) give a zero-package-manager
  fallback immediately, serving any consumer unwilling to add a vcpkg
  registry at all - a cheap, permanent safety net regardless of how the
  vcpkg port's adoption plays out.

### Negative

- `smbios_c`'s self-hosted vcpkg port is meaningfully less discoverable
  than a central-registry entry - a prospective consumer has to already
  know this project exists and read its docs to find the registry/overlay
  configuration, rather than finding it via `vcpkg search`.
- This ADR explicitly does **not** solve C++-level `libsmbios` packaging;
  any future "I want to link `libsmbios` from another CMake project, not
  just `smbios_c`'s C ABI" request is left unresolved by this decision and
  will need a second round of design work (install rules + an
  `INTERFACE`-link-safe exported target) before it can be packaged safely.
- Maintaining a vcpkg port (even self-hosted) adds a second place
  (alongside `smbios_c/CMakeLists.txt`'s own `SMBIOS_C_VERSION`) that must
  be bumped in lockstep on every `smbios_c` release, and a second platform
  matrix (vcpkg's own Windows/Linux/macOS/osx-arm64 triplets) this
  project's CI does not currently exercise end-to-end via the port itself
  (only via this repo's own `CMakePresets.json` presets).
- Keeping GitHub Releases artifacts *and* a vcpkg port means two release
  mechanisms to update on every `smbios_c` version bump instead of one,
  until/unless the releases artifacts are later retired in favor of the
  port alone.

## Validation / Rollout

- A follow-up roadmap milestone (number TBD, scheduled after milestone
  0009 closes) authors `ports/smbios-c/{portfile.cmake,vcpkg.json}` (or an
  equivalent sibling-repo registry layout) wrapping the existing
  `smbios_c/CMakeLists.txt` `install()` rules unchanged, plus a
  `vcpkg-configuration.json` `registries` entry for a scratch consumer
  project to add. Verified the same way Story 03.0 verified `pkg-config`
  discovery: a trivial external C/C++ program, with no reference to this
  repo's source tree, built via `vcpkg install smbios-c` (through the
  overlay/registry) and `find_package(smbios_c CONFIG)` or
  `pkg-config --cflags --libs smbios_c`, run successfully against real
  SMBIOS data on at least Linux and macOS (Windows once the port's own
  triplet support is confirmed).
- `nm -gU` (or the Windows/Linux equivalent) re-run against the
  vcpkg-built artifact specifically, not just this project's own preset
  build, to confirm the export allowlist (`smbios_c.exported_symbols`/
  `smbios_c.version`) survives vcpkg's own build invocation unchanged.
- The GitHub Releases baseline (option 3) is validated by downloading a
  tagged release's artifact on a clean machine with no vcpkg/Conan/this
  repo's source present and confirming `pkg-config --cflags --libs
  smbios_c` plus a compile/link/run smoke test succeeds from the
  extracted tarball alone.
- Promotion triggers, each independently gating the next step, are **not**
  scheduled as part of this rollout - they are explicit go/no-go checks for
  a *future* decision, not milestone exit criteria here:
  - Central vcpkg (`microsoft/vcpkg`) submission: only once a named real
    external consumer depends on the self-hosted port.
  - Any Conan work (2a or 2b): only once a named real external consumer
    specifically requests Conan, per DD3.
  - `libsmbios` C++-surface packaging: only once its own `install()` +
    whole-archive-safe package-config prerequisite work (named in
    Decision point 1) is designed and implemented as its own task.

## Links

- **Roadmap story/task:** _(none yet - this ADR is exploratory/forward-looking, written to pick back up the thread `docs/roadmap/0007-c-abi-shim-for-cross-language-interop/status.md` explicitly deferred; a concrete roadmap milestone is future work, not yet created)_
- **Supporting specs:** [docs/roadmap/0007-c-abi-shim-for-cross-language-interop/plan.md](/docs/roadmap/0007-c-abi-shim-for-cross-language-interop/plan.md) (Story 03.0's original vcpkg-port deferral and its trigger condition), [docs/roadmap/0007-c-abi-shim-for-cross-language-interop/status.md](/docs/roadmap/0007-c-abi-shim-for-cross-language-interop/status.md) (Story 03.0 Task 02's notes), `smbios_c/CMakeLists.txt` and `smbios_c/smbios_c.pc.in` (the existing install/pkg-config mechanism this ADR builds on), root `vcpkg.json` and `CMakePresets.json` (this project's own vcpkg-consumer tooling, reused as the model for packaging)
- **Diagrams:** _(none needed - no new dependency-graph shape, only a distribution-channel decision)_
