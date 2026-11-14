# Story 02.0 - Pinned Toolchain, Hardened Environment and Least Privilege

**Parent milestone:** [plan.md](../plan.md)
**Status:** ✅ Complete (2026-11-13; on-runner checks pending in Tasks 01-04)
**Depends on:** Story 01.0 Task 02 (the step runner). Task 01 can start in parallel with Story 01.0.
**Flag:** `security_sensitive`. The `security-auditor` reviews Task 03's policy rules and the threat model is written before Story 03.0's workflow code.

## Scope

This story removes the environmental causes behind milestone 0011's CI failures (plan E2, E3, E7):

- floating runner labels and tool versions;
- a dependency install that hangs silently when a mirror's DNS fails;
- unbounded step time;
- a workflow with no `permissions:` block.

It also adds the gate that keeps those properties from drifting back: actionlint plus a project policy check.

## Tasks

| #  | Document | Status | Blocks |
|----|----------|--------|--------|
| 01 | [Runner label and tool-version pinning; version header](01-runner-and-tool-version-pinning.md) | ✅ Complete (on-runner checks pending) | 02, Story 03.0 |
| 02 | [Dependency install robustness and vcpkg cache health](02-dependency-install-robustness-and-cache-health.md) | ✅ Complete (on-runner checks pending) | Story 03.0 |
| 03 | [Workflow lint and policy gate](03-workflow-lint-and-policy-gate.md) | ✅ Complete (P11 single-emitter hits fixed by Task 04: `lint.py` exits 0 locally; on-runner checks pending) | Story 03.0 |
| 04 | [Step-runner security follow-ups (environment scrub, data hygiene, output channel)](04-step-runner-security-follow-ups.md) | ✅ Complete (on-runner checks pending) | 02, 03, Story 03.0 Task 01 |

Task 04 was added 2026-11-13 from the [CI-rework threat model](/docs/security/2026-11-13-ci-rework.md) (per-task requirements 1-5). It amends Story 01.0's shipped runner code without reopening Story 01.0.

## Key constraints

- **Pin what decides behaviour, and record everything else.** Hosted images are updated weekly even under a fixed label. The milestone pins the label, CMake/CTest/Ninja and the vcpkg tool commit, and records `ImageOS`/`ImageVersion`, the compiler and Python in the RC4 header. It does not pretend to pin the image.
- **One CMake/CTest version on all three OSes.** The ctest 3.31-versus-4.4.5 split (E4) must not recur because of the environment, even though Story 03.0 also stops parsing ctest's wording.
- **Fail fast and visibly.** Any network-dependent step gets an explicit step timeout, tool-level timeouts and at most one retry. It prints one line saying which source was used. A stall must fail in minutes, with output, never at the job ceiling.
- **Least privilege.** `permissions: contents: read` at workflow level. No job gets more without a comment that names the reason. The `report` job (Story 04.0) needs nothing beyond that.

## Non-goals

- Self-hosted runners, container jobs, and caching apt packages (a cache of `.deb` files is a second stale-state source; the mirror fallback is enough).
- ~~Replacing `lukka/run-vcpkg` with a hand-written bootstrap.~~ **Reversed 2026-11-12 by [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md):** policy rule P13 rejects an action whose job a neutral script can do. Task 02 therefore adds `scripts/ci/bootstrap_vcpkg.py`, which clones the pinned vcpkg commit and runs `bootstrap-vcpkg`, and that script replaces the action. Task 01's pins (runner labels, CMake/CTest/Ninja, vcpkg commit) live in one data file that every pipeline stage reads, including 0012's package/verify/publish.

## Open questions

- Exact labels and versions are chosen when Task 01 runs, from GitHub's then-current `runner-images` list. Candidates: `ubuntu-24.04` (the label the 0011 apt fallback was verified on), `windows-2025`, `macos-15`. One constraint is checked, not assumed: the Windows image must provide the `Visual Studio 17 2022` generator that `CMakePresets.json`'s `windows` preset names.

## Files or directories created or modified by this story

- `scripts/ci/print_env.py`, `scripts/ci/apt_install.py`, `scripts/ci/vcpkg_cache_report.py`, `scripts/ci/bootstrap_vcpkg.py`, `scripts/ci/check_workflow_policy.py`, `scripts/ci/install_actionlint.py`, `scripts/ci/lint.py`, `scripts/ci/requirements-tools.txt`, the pins data file, `scripts/ci/actions_allowlist.json`, `scripts/ci/step_budgets.json` (new), plus their tests under `scripts/ci/tests/`.
- Task 04 modifies Story 01.0's `scripts/ci/ci_step.py`, `scripts/ci/reporters/`, `scripts/ci/env_names.py` and `scripts/ci/tests/fixtures/hostile.log`.
- No workflow file yet. The pinned values are consumed by Story 03.0's `ci-next.yml`.
