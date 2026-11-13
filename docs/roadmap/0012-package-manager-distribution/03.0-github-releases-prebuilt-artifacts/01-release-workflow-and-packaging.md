# 01 - Package stage: archive the tested install tree

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** milestone 0014 Story 03.0 Task 01 (`stage.py`, the
install-tree export SC2)

> **Amended 2026-11-12 by [ADR 0005](/docs/adr/0005-unified-build-package-publish-pipeline.md).**
> The original version of this task specified a standalone tag-triggered
> `release.yml` that rebuilt on `ubuntu-latest`/`macos-latest` and uploaded
> with a marketplace action. That design is withdrawn, for three reasons:
>
> - It broke milestone 0014's policy rules P3 (floating labels), P6/P10
>   (expressions in `run:`), P7 (multi-line shell) and P13 (an action
>   where a script suffices).
> - It shipped a build other than the tested one (ADR 0005 DD1).
> - It raced three uploads into one release (review M2).
>
> Uploading is now the `publish` stage (Story 04.0 Task 02).

## Objective

Implement `python3 scripts/ci/stage.py package --leg <leg>`. For one
packaging leg (`linux-x64`, `windows-x64`, `macos-arm64` - see 0014
`legs.json`), it takes the build stage's tested
`$CI_ARTIFACT_DIR/install/<leg>/` tree and produces the ADR 0005 D4 `dist/`
entries. It never configures or builds.

## Files

- **Create** `scripts/ci/package_archive.py` (stdlib only). It:
  - archives `install/<leg>/` as `.zip` on Windows legs and `.tar.gz`
    elsewhere, with sorted entries, normalised mode bits and a fixed mtime;
  - writes the `.sha256` file;
  - merges its leg into `dist/manifest.json` (fields per ADR 0005 D4,
    including the glibc / macOS minimum / MSVC toolset and the build
    configuration).
- **Modify** `scripts/ci/stage.py`: register the `package` stage. Its steps
  go through `ci_step` (RC1-RC3).
- **Create** `.github/workflows/stage-package.yml`: reusable, one
  `stage.py package` step per leg, plus artifact download/upload of
  `install/<leg>` and `dist/`. Every convention (pins, permissions,
  neutral `CI_*` env, step shape) is defined by ADR 0005 and 0014, and the
  policy gate checks it.

## Functions and Classes

```python
def archive_tree(src: Path, dest: Path, fmt: Literal["zip", "tar.gz"]) -> Path: ...
def sha256_file(path: Path) -> str: ...
def update_manifest(manifest: Path, leg: str, archive: Path, meta: Mapping[str, str]) -> None: ...
```

## Constraints

- **No rebuild and no hand-assembled files**: the archive content is
  exactly the `cmake --install` tree (ADR 0004 option 3, ADR 0005 D1).
- **The shipped build configuration is decided in this story** and
  recorded in `status.md` before implementation. The build stage's
  packaging legs then build and test that configuration (0014 SC2).
- **Whether the `smbios` CLI ships** in the tree is an open maintainer
  question (ADR 0005 Open questions 3). If the answer is yes, the CLI's
  `install()` rule is a scope addition to Story 01.0 Task 01, not part of
  this task.

## Tests

- `scripts/ci/tests/test_package_archive.py`: archiving the same tree twice
  gives byte-identical archives; the manifest records the correct digest;
  a missing `install/<leg>` exits 2 with one RC1 line.
- End-to-end: covered by Story 04.0's `verify` stage.

## Success criteria

- [ ] On a PR to `master`, `package` produces three archives, three
      `.sha256` files and one manifest from the tested trees, with no
      configure/build step in the stage.
- [ ] Each archive contains `smbios_c`'s library/headers/`.pc` and
      `libsmbios`'s installed library/headers/package config, confirmed by
      listing the archive contents.
- [ ] `stage.py package` runs locally against a local `install/<leg>/`
      with `CI_REPORTER=plain`.
