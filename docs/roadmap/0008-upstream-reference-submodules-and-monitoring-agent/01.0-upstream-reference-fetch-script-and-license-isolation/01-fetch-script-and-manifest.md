# 01 - Fetch script and manifest

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Add a tracked manifest of upstream references and a cross-platform Python
script that reproducibly clones (or fast-forwards) each one into the
git-ignored `c-versions/` directory. This replaces machine-local symlinks
without tracking any GPL code.

## Files

- **Create** `scripts/upstream_references.json`. This is the single source
  of truth for upstream URLs; the script reads it and never hard-codes a
  URL. It holds one object per upstream, with the following fields:

  | Field          | dmidecode_linux                              | dmidecode_macos                              |
  |----------------|-----------------------------------------------|-----------------------------------------------|
  | `name`         | `dmidecode_linux`                             | `dmidecode_macos`                             |
  | `path`         | `c-versions/dmidecode_linux`                  | `c-versions/dmidecode_macos`                  |
  | `url`          | `https://github.com/mirror/dmidecode.git`     | `https://github.com/acidanthera/dmidecode.git` |
  | `baseline_sha` | `b70f6ee0f2e879deebc630da20d27d350407889e`  | `334e53aae57632329adf11fe5e852749b038a793`  |

  `baseline_sha` is the last upstream commit that a watcher report
  (Story 03.0) has triaged. The seeds are both upstreams' HEADs on
  2026-11-09, verified via `git ls-remote`. They count as "triaged" by
  fiat, because milestones 0003-0005 were written against these
  checkouts.

  A SHA is metadata, not GPL content. The manifest never contains
  upstream source, diffs, or excerpts.

  URLs are HTTPS. SSH (`git@github.com:...`) would require credentials
  that no reader has for these third-party repositories.
- **Create** `scripts/fetch_upstream_references.py`. For each manifest
  entry it handles four cases:
  - **`path` absent:** run `git clone <url> <path>`, a **full** clone of
    the default branch (no `--depth`). The watcher needs `git log
    <baseline_sha>..HEAD` ranges, and dmidecode's history is small.
  - **`path` is an existing git work tree** (and not a symlink):
    1. Fetch.
    2. Fast-forward the current branch to its upstream (equivalent to
       `git pull --ff-only`).
    3. If the fast-forward is impossible (local commits, dirty tree, or a
       detached HEAD), warn, leave the checkout untouched, and record a
       failure for the exit status.
  - **`path` is a symlink or an existing non-git directory:** print a
    warning naming the path and skip it. Never delete, move, or overwrite
    it.
  - **After any of the above:** print `<name> <path> <HEAD SHA>` (or
    `skipped` / `failed`).

  The script runs every entry even when an earlier one fails, and exits
  non-zero if any entry failed. A skip is a warning, not a failure.
- **Modify** `CLAUDE.md`. Rewrite its `c-versions/` sentence to say:
  - `c-versions/` is populated by `python scripts/fetch_upstream_references.py`
    from `scripts/upstream_references.json`;
  - it is still untracked (git-ignored);
  - it is still not part of the build;
  - it is GPL reference material only.
- **Modify** `.gitignore`, comment only. The comment above `/c-versions/`
  currently says "milestone 0008 replaces these with pinned git
  submodules". Change it to say the directory is populated by the fetch
  script and that the ignore rule is load-bearing for GPL isolation. The
  `/c-versions/` rule itself must stay unchanged.

## Functions and Classes

N/A - repository tooling only. If the script exposes helpers, follow the
existing `scripts/*.py` conventions:
- module and function docstrings;
- `argparse` entry point with `main(argv) -> int`;
- `pathlib`;
- invoke `git` via `subprocess` with argument lists (no shell strings),
  so the script is portable to Windows.

## Constraints

- No `.gitmodules`, no `git submodule` commands, and no file under
  `c-versions/` ever staged.
- **Cross-platform:** run on the three CI OSes' stock Python 3. Use no
  third-party packages and resolve paths relative to the repo root (not
  the CWD).
- **Never destructive:** no `rm`, `reset --hard`, `clean`, or forced
  checkout.
- The checkout is meant to float at HEAD. The script never checks out
  `baseline_sha`, because the reviewed state lives in the manifest, not in
  the working copy.
- Requires network access. It is a developer/agent tool, so neither CI
  nor the build invokes it.

## Tests

- **Fresh state** (no `c-versions/`): one run produces both clones, and
  each printed SHA matches `git ls-remote <url> HEAD`.
- **Second run:** a no-op fast-forward that prints the same SHAs.
- **Symlink:** replace one path with a symlink. The run warns, skips it,
  and leaves the symlink intact.
- **Non-git directory:** same check with a plain non-git directory.
- **Clean tree:** after any of the above, `git status --porcelain` at the
  repo root is empty.

## Success criteria

- [ ] On a fresh clone of this repository, running
      `python scripts/fetch_upstream_references.py` produces working
      `c-versions/dmidecode_linux`/`dmidecode_macos` checkouts with full
      history.
- [ ] `git status` is clean afterwards (nothing under `c-versions/` shows
      as untracked or staged).
- [ ] No `.gitmodules` exists. The manifest uses HTTPS URLs and the seed
      `baseline_sha` values above.
- [ ] CLAUDE.md's `c-versions/` description reflects the script
      (untracked, populated by the script, not part of the build).
