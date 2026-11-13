# 02 - Triggers, concurrency and matrix tiering

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started
**Depends on:** maintainer decision Q1

## Objective

Spend runner time where it buys information (plan E7):

- no duplicate push and PR runs;
- superseded runs are cancelled;
- docs-only changes get a docs-only run;
- the full 3-OS matrix runs where Q1 says.

## Files

- **Modify** `ci.yml`:
  - **Triggers:**
    - `push` (all branches except `probe/**`);
    - `pull_request` to `master`;
    - `schedule` nightly (full matrix on `master`, cache save allowed, Story 02.0 Task 02);
    - `workflow_dispatch` with inputs `jobs` (choice: `all`, `lint`, `build`, `sanitizers`, `gui`) and `os` (choice: `all`, `linux`, `windows`, `macos`), which feeds Task 03's single-job runs.
  - **Duplicate PR runs:** status checks attach to the commit SHA, so the `push` run's results already show on a same-repo PR. A same-repo `pull_request` run (`github.event.pull_request.head.repo.full_name == github.repository`) therefore runs only the legs the push tier did **not** run: Windows/macOS under the recommended tiering, nothing beyond `lint` if Q1 keeps the full matrix on every push. Its report shows the other legs as `not run: covered by push run`. A fork PR (no push run in this repository) runs every leg.
  - **Concurrency:** `group: ci-${{ github.workflow }}-${{ github.event.pull_request.number || github.ref }}`, `cancel-in-progress: ${{ github.ref != 'refs/heads/master' }}`. `master` runs are never cancelled.
  - **Tiering:** a first `plan` job (Linux, at most 2 minutes, `needs` nothing) runs `classify_changes.py` and the tier table below, and outputs the matrix JSON that `build`/`sanitizers`/`gui` consume through `fromJSON`. `lint` always runs.
- **Create** `scripts/ci/classify_changes.py`:
  - It runs `git diff --name-only <base>...<head>`. The base is the PR base, or `github.event.before` on push. If the base is missing (new branch, force-push), the result is `full`.
  - It outputs `docs` or `full`. `docs` only when every path matches `docs/**` or a root-level `*.md`. Anything under `tests/data/` (goldens), `scripts/`, `examples/` or `.github/` is `full`, because C02 and C07 depend on it.
  - It prints one `ok: changes: <docs|full> (<n> paths)` line.

**Tier table** (recommended answer to Q1; replace it with the maintainer's ruling):

| Event | lint | build | sanitizers | gui |
|-------|------|-------|------------|-----|
| push, `docs` changes | yes | - | - | - |
| push to a non-`master` branch | yes | linux | linux | linux-gui |
| same-repo PR to `master` | yes | windows, macos | macos | windows-gui, macos-gui |
| fork PR to `master`, push to `master`, nightly, dispatch `all` | yes | 3 OS | 2 OS | 3 OS |
| dispatch with `jobs`/`os` set | yes | as selected | as selected | as selected |

**ADR 0005 extension (2026-11-12; the Q1 ruling above is unchanged).** These columns are added for the pipeline stages that milestone 0012 defines. Until 0012 lands, the report shows them as `not run: stage not defined`.

| Event | package | verify | publish |
|-------|---------|--------|---------|
| push (any branch), docs-only | - | - | - |
| same-repo PR to `master` | yes (all packaging legs; the Linux packaging leg is rebuilt in this run) | yes | - |
| fork PR, push to `master`, nightly, dispatch `all` | yes | yes | - |
| tag `v*` (new trigger: `push: tags: ['v*']`) | yes | yes | yes |

- **Tag runs** always run the full matrix and are never cancelled: the concurrency `cancel-in-progress` expression also excludes `refs/tags/`.
- **Dispatch never reaches `publish`.** `matrix_for` receives `CI_EVENT`, never a GitHub event name, so the same function serves a TeamCity adapter (ADR 0005 Platform portability).
- **Tiering is neutral.** `plan.py`/`classify_changes.py` emit a per-leg run/skip plan, which a leg can also consult itself and then print `skip: tier`.

## Functions and Classes

```python
def changed_paths(base: str | None, head: str) -> list[str] | None: ...  # None -> full
def classify(paths: Sequence[str] | None) -> Literal["docs", "full"]: ...
def matrix_for(event: str, ref: str, classification: str, jobs: str, os: str) -> dict: ...
```

## Constraints

- **No `paths:`/`paths-ignore:` filters on `on:`.** A workflow skipped by path never reports, which leaves required checks pending (Q5). Classification happens inside the run, and the skipped legs are reported.
- The `plan` job's outputs are JSON built by a script and passed through `fromJSON`. No expression concatenates user-controlled strings, such as branch names.
- The nightly schedule runs only on the default branch, by GitHub's own rule, and is the only event besides `master` pushes that saves the vcpkg cache.

## Tests

- `scripts/ci/tests/test_classify_changes.py`: docs-only gives `docs`; a golden file under `tests/data/` gives `full`; a missing base gives `full`; and the tier table is checked row by row through `matrix_for`.
- On real runs:
  - a docs-only push runs `lint` only, and the report shows the others as `not run: tier`;
  - two quick pushes to one branch -> the first run is cancelled;
  - a dispatch with `jobs=gui os=windows` runs `lint` + the one GUI leg.

## Success criteria

- [ ] No leg runs twice for one commit (push + same-repo PR), and every leg runs at least once for a commit that has an open PR to `master`.
- [ ] Superseded non-`master` runs are cancelled.
- [ ] The tier table in `docs/dev/ci.md` matches the ruling and `matrix_for`.
