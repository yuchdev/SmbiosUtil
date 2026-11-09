# 02 - CI submodule checkout verification

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** Task 01

## Objective

Add a CI step that checks out submodules recursively and verifies the
working tree matches `.gitmodules`' pinned commits exactly - catching the
case where someone manually `git checkout`s a submodule to an
unpinned commit locally and forgets to either commit that change
deliberately or revert it.

## Files

- **Modify** `.github/workflows/ci.yml` (milestone 0001):
  ```yaml
  - uses: actions/checkout@<pinned-sha>
    with:
      submodules: recursive
  - name: Verify submodule pins
    run: |
      git submodule status | grep -E '^[+-]' && { echo "Submodule not at pinned commit"; exit 1; } || true
  ```

## Functions and Classes

N/A - CI configuration only.

## Constraints

- This check is about **drift from the pin**, not about whether the pin
  itself is stale (Story 03.0's agent handles "is there something newer
  worth having," which is a different, non-blocking question) - this step
  fails the build; the agent's reports never do.

## Tests

- A deliberate local drift (checking out a different commit in one
  submodule without updating the pin) run through this step locally,
  confirming it fails with the expected message.

## Success criteria

- [ ] CI fails if a submodule's checked-out commit doesn't match its pin.
- [ ] CI passes on the current (correctly pinned) tree.
