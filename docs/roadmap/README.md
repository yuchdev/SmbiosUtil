# libbios Roadmap

Planning and progress tracking for libbios, organized as a three-tier
hierarchy: **Milestone → Story → Task**.

## Hierarchy & vocabulary

| Tier          | Meaning                                                                                                       | Lives in                                                |
|---------------|-----------------------------------------------------------------------------------------------------------------|----------------------------------------------------------|
| **Milestone** | A large, strategic initiative / development direction (e.g. the generic implementation effort).               | A folder `docs/roadmap/{NNNN}-{milestone-slug}/`.       |
| **Story**     | One deliverable unit of a milestone (e.g. "AWS Bedrock Backend"). Listed in the milestone's `## Stories` table. | A subfolder `…/{TT.t}-{story-slug}/` with a `README.md`. |
| **Task**      | An atomic, implementable spec (one file, one set of classes/tests). Listed in the story's `## Tasks` table. | A file `…/{TT.t}-{story-slug}/{NN}-{task-slug}.md`.   |

A *task* is part of a *story*; a *story* is part of a *milestone*.

## File & folder convention

```
docs/roadmap/
  README.md                              ← this index + convention
  {NNNN}-{milestone-slug}/               ← MILESTONE
    plan.md                              ← milestone spec; opens with a `## Stories` table
    status.md                            ← progress tracker (optional); `## Current status` table
    {TT.t}-{story-slug}/                 ← STORY
      README.md                          ← story spec; opens with a `## Tasks` table
      {NN}-{task-slug}.md                ← TASK spec
```

- `{NNNN}` - zero-padded milestone number (`0001`, `0002`, …).
- `{TT.t}` - story number, carried from the milestone's `## Stories` table (e.g. `03.2`, `06.1`, `08.0`).
- `{NN}` - zero-padded task order (`01`, `02`, …).
- Slugs are kebab-case.

### Heading vocabulary

- A milestone's `plan.md` lists its stories under a `## Stories` heading (table column: `Story`).
- A story's `README.md` lists its tasks under a `## Tasks` heading.

### Linking convention

When a document references another **specific** document, use an
absolute-from-repo-root Markdown link:

```
[docs/roadmap/0001-portable-build-system-and-dependencies/plan.md](/docs/roadmap/0001-portable-build-system-and-dependencies/plan.md)
```

- Always a leading `/` (repo root), never relative `../../` chains.
- **Template** paths (containing `{` / `}`, e.g. `` `{NN}-{task-slug}.md` ``) stay as
  backtick code spans, not links.
- Run `python scripts/check_doc_links.py docs/` (or the `/link-check` skill) to verify every
  link target and `#heading-anchor` resolves. The `doc_link_check` hook runs it on edits.

## Program overview

This roadmap takes `libbios` from a two-structure, Windows/pre-UEFI-Linux
prototype to a portable, UEFI-capable C++20 library with a modern,
dependency-managed build, and a C ABI alongside its C++ one. The seven
milestones below are intentionally sequential - each is the foundation the
next one is built on:

1. **0001** gives every later milestone a working, reproducible,
   vcpkg-based build before any source code changes land.
2. **0002** raises the language standard and fixes the latent bugs already
   known in the current prototype (see
   [plan.md](/docs/roadmap/0002-cpp20-modernization-and-bugfixes/plan.md))
   so later milestones build on correct, `-Wall -Wextra -Werror`-clean code.
3. **0003** replaces the `reinterpret_cast`-onto-packed-struct parsing
   technique - the part of the prototype that is not portable to a
   big-endian host and does not scale past a handful of hand-written
   version subclasses - with endian-safe, bounds-checked field accessors.
4. **0004** rebuilds the type-to-class factory as a self-registering one and
   uses the new accessor pattern from 0003 to cover the SMBIOS structure
   types the prototype does not implement yet.
5. **0005** is the other headline goal: a portable `ISmbiosSource` strategy
   interface, a real (non-stubbed) Linux implementation, and genuinely new
   UEFI-based sources for Linux and **macOS**, which today has none.
6. **0006** closes the core program: a hermetic, fixture-driven test layer
   that does not require root/admin privileges to run in CI, boundary/
   malformed-table tests for the 0003 parser, and a documentation pass.
7. **0007** adds a C ABI alongside the C++ one - a thin shim over opaque
   handles wrapping the finished C++ core (depends on 0004's entry model
   being settled), so this library is callable from Python/Rust/C#/Go via
   FFI, not just from other C++ code.

## Milestones

| #    | Milestone                                                             | Spec                                                                     | Status                                                                     |
|------|------------------------------------------------------------------------|---------------------------------------------------------------------------|-----------------------------------------------------------------------------|
| 0001 | Portable build system and dependencies (vcpkg, CMake presets, CI)     | [plan.md](./0001-portable-build-system-and-dependencies/plan.md)       | [status.md](./0001-portable-build-system-and-dependencies/status.md)     |
| 0002 | C++20 modernization and latent bug remediation                        | [plan.md](./0002-cpp20-modernization-and-bugfixes/plan.md)              | [status.md](./0002-cpp20-modernization-and-bugfixes/status.md)           |
| 0003 | Portable, endian-safe, bounds-checked SMBIOS byte-level parsing        | [plan.md](./0003-portable-byte-level-smbios-parsing/plan.md)           | [status.md](./0003-portable-byte-level-smbios-parsing/status.md)         |
| 0004 | Entry model & factory modernization; expanded structure coverage      | [plan.md](./0004-entry-model-and-factory-modernization/plan.md)        | [status.md](./0004-entry-model-and-factory-modernization/status.md)      |
| 0005 | Native source abstraction and UEFI support (Linux & macOS)            | [plan.md](./0005-uefi-and-native-source-abstraction/plan.md)           | [status.md](./0005-uefi-and-native-source-abstraction/status.md)         |
| 0006 | Hermetic testing, boundary hardening and documentation                | [plan.md](./0006-testing-hardening-and-documentation/plan.md)          | [status.md](./0006-testing-hardening-and-documentation/status.md)        |
| 0007 | C ABI shim for cross-language interop (opaque handles over the C++ core) | [plan.md](./0007-c-abi-shim-for-cross-language-interop/plan.md)     | [status.md](./0007-c-abi-shim-for-cross-language-interop/status.md)      |

**Legend (status.md files):** ✅ Complete · 🔶 In progress / partial · ⬜ Not started
