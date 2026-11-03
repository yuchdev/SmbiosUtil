# 01 - Add confirmed submodules

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Remove the two symlinks under `c-versions/` and add real git submodules
at the same paths, pinned to the confirmed upstream commits.

## Files

- **Remove** `c-versions/dmidecode_linux`, `c-versions/dmidecode_macos`
  (symlinks).
- **Create** `.gitmodules`:
  ```
  [submodule "c-versions/dmidecode_linux"]
      path = c-versions/dmidecode_linux
      url = https://github.com/mirror/dmidecode.git
  [submodule "c-versions/dmidecode_macos"]
      path = c-versions/dmidecode_macos
      url = https://github.com/acidanthera/dmidecode.git
  ```
  (`https://` URLs, not `git@github.com:...` SSH - a submodule URL that
  requires SSH credentials breaks a plain `git clone` for anyone without
  push access to those third-party repos, which is everyone; read access
  only needs HTTPS.)
- **Run** `git submodule add <url> c-versions/dmidecode_linux` /
  `dmidecode_macos`, then `git submodule update --init` and pin each to
  the commit currently checked out in the existing symlinked working
  copies (`b70f6ee` for Linux, `334e53a` for macOS, per the commits
  observed when these upstreams were first confirmed - re-verify the
  exact SHA at implementation time rather than trusting this summary, in
  case either upstream has moved since this spec was written).

## Functions and Classes

N/A - repository structure only.

## Constraints

- Pin to an explicit commit, not `HEAD` of any branch - `git submodule`
  always pins to a specific commit by design, but confirm the chosen
  commit is a real, stable point (e.g. a tagged release or a commit on
  the default branch) rather than an arbitrary in-progress commit on a
  feature branch of either upstream.
- CLAUDE.md's existing sentence describing `c-versions/` ("untracked
  reference material... It is not part of the build") needs a small edit
  here: it is no longer *untracked* (it is now tracked, as submodules) -
  "not part of the build" stays exactly as true as before.

## Tests

- `git submodule status` shows both submodules checked out at their
  pinned commits with no `+`/`-` prefix (meaning the working tree matches
  the pinned commit exactly).

## Success criteria

- [ ] `git clone --recurse-submodules <this repo>` on a fresh machine
      (no pre-existing symlink targets) produces working
      `c-versions/dmidecode_linux`/`dmidecode_macos` checkouts.
- [ ] `.gitmodules` uses HTTPS URLs.
- [ ] CLAUDE.md's `c-versions/` description is updated (tracked, not
      untracked; still not part of the build).
