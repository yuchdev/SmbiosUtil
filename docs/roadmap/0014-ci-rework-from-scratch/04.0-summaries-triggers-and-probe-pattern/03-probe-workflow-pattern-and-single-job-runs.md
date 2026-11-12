# 03 - Probe-workflow pattern and single-job runs

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Story 02.0 (pinned labels), Task 02 (dispatch inputs)

## Objective

Cut the cost of debugging CI itself from a ~30-minute full cycle to minutes (plan E6). This standardises what milestone 0011 improvised twice: a DNS probe and a ctest-wording probe, each in a throwaway workflow that was deleted afterwards.

There are two tools:

1. **Single-job run**, for "does job X pass now?". Use `gh workflow run ci.yml --ref <branch> -f jobs=<job> -f os=<os>`, from Task 02's dispatch inputs. This only works once `ci.yml` with these inputs is on the default branch. Until then, push to the branch and rely on tiering.
2. **Probe workflow**, for "what is true on the runner?" (DNS, tool versions, a file's mode). A copy of `docs/dev/ci-probe-template.yml` goes on a `probe/<topic>` branch as `.github/workflows/probe-<topic>.yml`. It runs only on that branch, prints only what the probe needs, and is deleted with the branch.

## Files

- **Create** `docs/dev/ci-probe-template.yml`. A workflow skeleton that already satisfies the policy gate:
  - `on: push: branches: ['probe/**']` and `workflow_dispatch`;
  - `permissions: contents: read`;
  - one job with a matrix of the pinned labels (comment out the ones you do not need);
  - `timeout-minutes: 10` on the job and each step;
  - checkout (SHA-pinned) -> `print_env.py` (RC4) -> one marked `# PROBE STEPS` section whose example steps go through `ci_step.py`. Examples: a DNS probe of the apt hosts, and `ctest --version`.
  - A header comment states the rules below.
- **Modify** `scripts/ci/check_workflow_policy.py` (P9): on any branch other than `probe/**`, a `probe-*.yml` under `.github/workflows/` is a violation, `stray probe workflow: expected 0 -> 1 (<file>)`. Because `ci.yml` ignores `probe/**` branches, the check fires as soon as a probe file reaches any branch that CI runs on, before it can reach `master`.

## Functions and Classes

None beyond the P9 extension.

## Constraints

- **Probe rules** (in the template header and in `docs/dev/ci.md`):
  1. Branch `probe/<topic>`, never merged.
  2. One question per probe. Print only the answer, in RC1 form.
  3. No secrets, no `permissions` beyond `contents: read`, and no firmware values. A probe that reads the live table prints counts only.
  4. Record the answer and the run ID in the relevant `status.md`, then delete the branch. This is the 0011 practice ("the probe workflows were deleted").
- The template lives under `docs/`, not `.github/workflows/`, so it never runs by itself. The policy gate runs on it as a fixture, so it stays compliant when the rules change.

## Tests

- `scripts/ci/tests/test_check_workflow_policy.py`: the template passes P1-P8; a `probe-x.yml` fixture on a non-probe branch fails P9.
- One real probe, done as this task's proof: `probe/runner-versions` prints RC4 on all pinned labels, and the time from push to answer is recorded (target under 5 minutes). The branch is deleted afterwards.

## Success criteria

- [ ] Template committed and policy-clean; P9 catches a stray probe.
- [ ] One probe and one single-job dispatch performed, with wall-clock times recorded in `status.md`.
