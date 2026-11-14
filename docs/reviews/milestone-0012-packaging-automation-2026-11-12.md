# Milestone 0012 packaging automation review - 2026-11-12

## Scope

Reviewed [milestone 0012](/docs/roadmap/0012-package-manager-distribution/plan.md)
(plan, status, Stories 01.0-03.0 and every task file) against
[ADR 0004](/docs/adr/0004-package-manager-distribution.md) and the current tree
(`CMakeLists.txt`, `smbios_c/CMakeLists.txt`, `smbios_c/smbios_c.pc.in`,
`.github/workflows/ci.yml`, `vcpkg-configuration.json`, milestone 0014's plan).
Question asked: is the path from packaging to publishing automated?

State of the tree: nothing from 0012 is implemented (all stories "Not started";
no `ports/`, `versions/`, `release.yml`, or `libsmbios` `install()` rules; the
`smbios` CLI has no `install()` rule either).

## Verdict

**Not fully automated as specified.** Story 03.0 automates the *packaging* half
(tag push -> build -> archive -> upload). Story 02.0 automates nothing: the vcpkg
registry update (port version, `x-add-version`, `git-tree`, baseline) is a
documented manual procedure, and nothing ties the tag, `SMBIOS_C_VERSION`, port
`vcpkg.json` and registry together. Nothing verifies a published artifact after
publication. Gap closed by a new **Story 04.0 - Release and publish
automation** (added to the milestone in this change).

## Findings

### HIGH

**H1 - The release tarball's `smbios_c.pc` will be wrong.** `smbios_c/smbios_c.pc.in`
sets `prefix=@CMAKE_INSTALL_PREFIX@`, substituted at *configure* time. Story
03.0 Task 01 configures with the default prefix and then runs
`cmake --install ... --prefix dist`, so the shipped `.pc` points at the build
machine's configure-time prefix, not the extraction directory. Task 03's
`pkg-config` smoke test would pass only if run with the original prefix present
or `--define-variable=prefix`. Evidence: the `.pc.in` and install block above.
The spec also forbids changing `smbios_c.pc.in` (plan.md Key constraints), so the
conflict is unresolved. Fix options: configure with `-DCMAKE_INSTALL_PREFIX` per
artifact (not relocatable), or make the `.pc` relocatable (`pcfiledir`-relative
prefix) - needs a ratified exception to the "unchanged" constraint.

**H2 - Registry publication is circular and manual.** The port's `REF` must be a
commit/tag of this repo containing the sources; the `git-tree` in
`versions/s-/smbios-c.json` is the tree hash of `ports/smbios-c/` at a commit
that already contains the final port files. So a release needs: (1) commit
source, (2) tag, (3) commit port with `REF`, (4) run `vcpkg x-add-version`,
(5) commit versions, and consumers pin to (5). Story 02.0 Task 02 documents
steps by hand ("commit both together"); none of it runs in CI, and a missed
step ships a registry that cannot resolve.

**H3 - No LICENSE file in the repo.** `ls LICENSE*` finds nothing. The port
spec uses `"license": "MIT"` and `vcpkg_install_copyright(FILE_LIST
"${SOURCE_PATH}/LICENSE")`; the portfile would fail, and the MIT claim is
unverified (not stated anywhere I checked). Licence choice is the maintainer's;
this blocks Story 02.0 and the license field of every artifact.

### MEDIUM

**M1 - Version sources can drift, by design.** Tag (`v*`), `SMBIOS_C_VERSION`
(`smbios_c/CMakeLists.txt:114`), port `vcpkg.json` `"version"`, `versions/`
baseline and the proposed `LIBSMBIOS_VERSION` are five independent values; Task
02 says drift is acceptable. For an automated publish this must be one source
with a CI check, otherwise the registry can serve version X built from tag Y.
(The existing `-DGIT_TAG=ON` scheme also creates tags `ver.major.minor...`; a
second tag scheme should be explicitly ruled out of the release trigger.)

**M2 - Three matrix jobs uploading to one release race.** Task 01 has each OS
job call `softprops/action-gh-release`; concurrent creation of the same release
is a known flake. Needs one create/draft job, matrix upload, one publish job.
There is also no gate that the tagged commit passed CI, no release notes, and
no draft -> published promotion.

**M3 - Task 03 verification is manual and pre-publication only.** "Clean machine"
checks and `verify_release_artifact.sh` are not wired into any workflow, and
nothing checks the *published* release (download URL + `.sha256`) afterwards.
Story 02.0 Task 03's consumer test needs the registry commit to exist, so it
cannot run on a PR unless the registry is tested via a filesystem registry
(`"kind": "filesystem"`) first.

**M4 - Portfile issues to verify.** `vcpkg_from_git` takes `URL`/`REF`; I believe
it has no `SHA512` argument (that belongs to `vcpkg_from_github`) - unverified,
check against the vcpkg docs when implementing. Also: `<org>` placeholders (real
remote is `yuchdev/SmbiosSdk`); `smbios_c` is `SHARED` but no
`vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)`/static-triplet handling (e.g.
`x64-windows-static`) is specified; no `usage` file or CMake package config for
`smbios_c`, so vcpkg consumers get only `pkg-config`, which is not the usual
vcpkg/Windows path.

**M5 - Portfile configures the whole root project.** Root `CMakeLists.txt` adds
the CLI, examples and tests unconditionally (only GUI/fuzzers/examples have
options). The port lists only `boost-iostreams`/`boost-date-time`; the CLI needs
`boost-program-options` and the tests `boost-test`. Whether root configure
succeeds without them in a vcpkg build sandbox is **unknown** - needs a trial
build; a build-options switch (e.g. `SMBIOS_BUILD_CLI`/`SMBIOS_BUILD_TESTS`)
may be required, which is a root `CMakeLists.txt` change the spec does not
budget for.

**M6 - Floating runners and arch.** `ubuntu-latest`/`macos-latest` give an
unpinned glibc baseline (Linux artifact may not run on older distros) and
arm64-only macOS; milestone 0014 (E2) already pins runners. The artifact
matrix should state glibc/arch support, or record "arm64 only".

### LOW / INFO

- **L1** - `shasum -a 256` (Task 02) does not exist on the Windows runner; use
  a Python/`certutil`/PowerShell form, and zip rather than `tar.gz` there.
- **L2** - Third-party actions are `<pinned-sha>` placeholders; the workflow
  needs an explicit `permissions: contents: write` (and nothing else). No build
  provenance/attestation or signing - checksums alone prove integrity only
  against a separately obtained hash. Optional: `actions/attest-build-provenance`.
- **L3** - Boost version skew: the registry port builds against the *consumer's*
  vcpkg baseline's Boost, not this repo's pinned baseline. Record a tested range.
- **I1** - Sequencing with milestone 0014: its plan says to land first and have
  `release.yml` adopt RC1-RC5 (`scripts/ci/ci_step.py`, policy gate). Neither is
  started; Story 04.0 inherits that ordering.
- **I2** - Whether to install the `smbios` CLI in the tarball is left open in
  Task 01; it needs a decision (recorded as an open question below).

## Unknowns needing a maintainer decision

1. Licence (H3).
2. `.pc` relocatability approach and permission to touch `smbios_c.pc.in` (H1).
3. Single version source: which file is authoritative (suggest the tag, with
   CMake/port values generated or checked from it) (M1).
4. Does the tarball include the `smbios` CLI? (I2)
5. Is vcpkg's windows static triplet a supported target? (M4)
6. Registry hosting: `main` of this repo as registry (consumers pin a SHA) vs a
   dedicated `vcpkg-registry` repo, which would break the circularity in H2.

## Action taken

Added Story 04.0 "Release and publish automation" to milestone 0012
(`plan.md`, `status.md`, story folder).
No product code was changed.
