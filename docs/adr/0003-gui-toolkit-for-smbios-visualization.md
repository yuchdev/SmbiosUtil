# 0003 - GUI toolkit for SMBIOS visualization

> **Status:** Proposed
>
> **Date:** 2026-11-10
>
> **Supersedes:** _(none)_
>
> **Superseded by:** _(none)_

## Context

Today, every way to look at a parsed SMBIOS table is textual: the `smbios`
CLI (`smbios/src/main.cpp`, `command_line_parser.cpp`) renders every
structure via `AbstractSMBiosEntry::render_to_description()` (the default
`text` format) or, since milestone 0009, one of `IOutputFormatter`'s other
registered formats (`xml`, `json`, soon `yaml`, plus any third-party format
registered via `REGISTER_OUTPUT_FORMATTER`). There is no way to *browse* a
table - expand a structure, inspect one field, compare two memory devices
side by side - without piping that text through another tool. This ADR is
exploratory: no roadmap milestone for a GUI exists yet (milestones 0001-0010
are all either closed or about to close; see
[docs/roadmap/README.md](/docs/roadmap/README.md)). Its purpose is to give
the user a basis for deciding whether, and how, to pursue one once milestone
0009 closes - **no implementation follows from this ADR by itself.**

### The seam a GUI would actually consume

Milestone 0009 (see `docs/roadmap/0009-structured-output-formats/plan.md`)
already built the right seam for this, for a different consumer (text/XML/
JSON/YAML renderers), and a GUI should use the same one rather than
reinventing a second decode path:

- `smbios::SMBios` (`libsmbios/include/libsmbios/smbios.hpp`) exposes
  bidirectional `begin()`/`end()` iterators over cached `DMIHeader`s
  (`get_type()`, `get_length()`), `get_smbios_version()`, and
  `render_to_description()` for the whole table.
- `SMBiosEntryFactory::create(header, version)`
  (`libsmbios/include/libsmbios/smbios_entry_factory.hpp`) turns one
  `DMIHeader` into a typed `AbstractSMBiosEntry` subclass (45 of them as of
  milestone 0010), or `nullptr` for an unregistered type.
- `AbstractSMBiosEntry::get_fields() -> std::vector<NamedField>`
  (`libsmbios/include/libsmbios/field_value.hpp`, milestone 0009 contract
  C1) is the one, generic, per-entry data shape every non-text renderer
  already walks: `NamedField{name, FieldValue}`, where `FieldValue` is
  `std::variant<std::string, uint64_t, bool, std::vector<uint8_t>>`. A GUI's
  tree/table/property-grid view can walk exactly this - one row per
  `NamedField` - without knowing anything about any individual structure
  type, the same way the JSON/XML renderers already don't (0009's R4).
- `smbios::IOutputFormatter` + `get_output_formatter()`/
  `registered_output_formatter_names()`
  (`libsmbios/include/libsmbios/output_formatter.hpp`) let a GUI offer
  "copy this entry as JSON/XML/YAML" by calling an existing registered
  formatter, instead of embedding a second JSON/XML writer.

Critically, **none of this is exposed through `smbios_c`**, the C ABI shim
(milestone 0007). `smbios_c` wraps only a curated subset of typed getters
(`smbios_bios_vendor()`, `smbios_memory_device_size()`, etc. - see
`docs/roadmap/0007-c-abi-shim-for-cross-language-interop/plan.md`'s Key
constraints) chosen before the structured field model existed, and exposing
`get_fields()`/`IOutputFormatter` through it is explicitly listed as a
**non-goal** of milestone 0009 ("a possible follow-up milestone, not this
one" - `docs/roadmap/0009-structured-output-formats/plan.md`, Non-goals).
So: any GUI technology that cannot link `libsmbios`'s C++ directly today has
*less* data to work with than the CLI already has, not the same amount
through a different door.

### Decision drivers

- **DD1 - Consume the real seam, not rendered text.** The GUI must walk
  `get_fields()`/`SMBios` iteration directly, in C++, the same way 0009's
  own renderers do (R4) - never re-parse `render_to_description()`'s prose,
  which is precisely the `dmidecode | awk` anti-pattern milestone 0009 exists
  to eliminate for every *other* consumer.
- **DD2 - Packaging weight proportional to a solo/small-team OSS project.**
  Every build product today is a static library plus one deliberately
  `SHARED` C ABI shim with an explicit export allowlist
  (`smbios_c.exported_symbols`/`smbios_c.version`) - there is no precedent
  here for a heavy runtime dependency, and no CI/packaging infrastructure
  for one beyond what `vcpkg.json` + `CMakePresets.json` already resolve.
- **DD3 - Licensing must not put a copyleft or commercial obligation onto
  `libsmbios`'s own link graph.** This repo has no top-level `LICENSE` file
  yet, but it already draws a hard line against GPL code reaching the build
  graph: `c-versions/` (upstream `dmidecode`, GPL-2.0-or-later) is
  explicitly reference-only, never compiled or linked (see root `CLAUDE.md`
  and `docs/dev/upstream_reference_review.md`). A GUI dependency that would
  require the whole `smbios`/`libsmbios` link graph, or redistribution of
  it, to go copyleft or commercially licensed is a strong rejection signal,
  not a minor con to note in passing.
- **DD4 - Do not design around work that does not exist.** No Rust, Python,
  or C# binding for this project exists today; milestone 0007's own
  status notes name one only as a future possibility ("revisit ... once a
  real external consumer (the Python/Rust/C# binding this milestone's own
  motivation names) actually needs it" -
  `docs/roadmap/0007-c-abi-shim-for-cross-language-interop/status.md`). Any
  option that depends on such a binding is speculative on *two* unstarted
  pieces of work (the binding itself, and extending `smbios_c` to expose
  `get_fields()`/formatters per DD1), not one.
- **DD5 - Native look-and-feel vs. ongoing maintenance cost, weighed
  honestly for a small/solo maintainer team.** Recent history on this branch
  is a single contributor; a GUI toolkit that demands ongoing
  signal/slot-lifecycle, MOC/UI-compiler, or multi-file-per-widget
  maintenance is a materially different ongoing cost than one that is not.
- **DD6 - Fit the existing build pipeline.** The GUI target must slot into
  the existing `CMakePresets.json` (`windows`/`linux`/`macos` presets, the
  vcpkg toolchain file, Boost already pinned via `vcpkg.json`) rather than
  introducing a second build or dependency-resolution mechanism.

## Decision

Build the future GUI on **Dear ImGui**, as a new, independent CMake target
(provisionally `smbios_gui`) added only once a real roadmap milestone picks
this up - this ADR records the toolkit choice, not a commitment to schedule
that milestone.

The target would link `libsmbios` directly as a whole archive (the same
`-force_load`/`/WHOLEARCHIVE`/`--whole-archive` pattern
`smbios/src/CMakeLists.txt` already uses, and for the identical reason:
`REGISTER_SMBIOS_ENTRY`'s and `REGISTER_OUTPUT_FORMATTER`'s static
initializers only run for object files actually linked into the final
binary). The view model is a direct read of the existing seam, not a new
one: a tree view keyed by `SMBios` iteration + `DMIHeader::get_type()` (one
node per structure, labeled via the existing `SMBiosHandler` enum), expanding
into one row per `NamedField` from `get_fields()` with its `FieldValue`
rendered by a `std::visit` over the variant (string as-is, `uint64_t` as a
decimal/hex-toggle, `bool` as a checkbox glyph, `vector<uint8_t>` as a hex
dump) - and a "copy as..." action that calls `get_output_formatter(name)`
for whichever format the registry has, rather than embedding a second JSON/
XML/YAML writer inside the GUI.

This satisfies every decision driver without new prospective work: DD1 (it
is a direct C++ consumer of `get_fields()`/`SMBios`/`IOutputFormatter`, no
rendered-text re-parsing), DD2 (Dear ImGui plus a single windowing/GPU
backend, e.g. SDL2 or GLFW + a native/Vulkan/Metal/D3D11 renderer backend,
whichever vcpkg already resolves per platform, is a small addition against
a project that already pulls in Boost via vcpkg), DD3 (MIT-licensed, no
copyleft/commercial clause), DD4 (no binding of any kind is required - it
links `libsmbios`'s existing C++ headers exactly as the `smbios` CLI and the
unit tests already do), DD5 (immediate-mode has no persistent widget tree,
signal/slot wiring, or UI-compiler step to maintain - the entire view is
redrawn from `get_fields()` every frame, which is also a natural fit for a
read-only inspection tool that never needs partial, incremental UI updates),
and DD6 (one more CMake target beside the existing `smbios`/`smbios_c`
ones, resolved the same way via the existing vcpkg manifest).

The explicit trade this decision makes: Dear ImGui does **not** produce a
platform-native look-and-feel (no Windows/macOS/GTK theming, no
accessibility-API integration, no native menu bar). For a read-mostly
diagnostic/inspection tool - not a shipped end-user application competing on
platform-idiomatic chrome - this is judged an acceptable trade against the
alternatives' real packaging and maintenance costs (see Alternatives below).
If a future maintainer later needs native chrome (accessibility compliance,
a corporate UX mandate, etc.), that is grounds to revisit this ADR, not to
silently drift from it.

## Alternatives Considered

| Alternative | Pros | Cons | Reason rejected |
|-------------|------|------|-----------------|
| **A - Status quo**: no GUI, `smbios --format` text/XML/JSON/YAML only | Zero new work, zero new dependency, zero new maintenance surface | Does not satisfy the user's "visually inspect/browse interactively" ask at all; every comparison (a second memory device, a BIOS characteristic bit) still requires piping text through another tool | Rejected as a real option - kept as the honest baseline this ADR is choosing to move away from |
| **B - Recommended: Dear ImGui** - immediate-mode, C++, links `libsmbios` directly | Direct C++ link (DD1/DD4, no C ABI hop); MIT license (DD3); small dependency footprint (one widget library + one windowing/GPU backend) fitting a solo-maintainer project (DD2); no persistent widget-tree/signal-slot lifecycle to maintain (DD5); fits the existing CMake+vcpkg pipeline (DD6) | No native look-and-feel (no platform theming, no native accessibility API, no native menu bar); typically needs a GPU-capable rendering backend (SDL2/GLFW + a graphics API) even for a mostly-static inspector UI, which is a heavier runtime assumption than Qt/wx make for pure widget rendering | Adopted |
| **C - Qt Widgets** - native look, heavier dependency, LGPL/commercial licensing | Mature native-looking widgets (tree view, property grid) out of the box; accessibility support; large ecosystem/tooling (Qt Creator, Qt Designer) | LGPL (or commercial) licensing requires care around static linking and redistribution obligations that this repo's own GPL-avoidance stance (DD3) was built to avoid extending to; MOC code-generation step is a new build-pipeline concept (DD6); heaviest runtime/toolchain footprint of all four options, a real ongoing cost for a solo/small maintainer (DD2/DD5) | Rejected - DD3's licensing bar and DD2/DD5's weight/maintenance bar both argue against it; native look-and-feel alone does not outweigh them for a diagnostic tool |
| **D - wxWidgets** - native look, lighter than Qt, less modern ecosystem | Native platform widgets without Qt's MOC step; permissive (wxWindows) license, so DD3 is satisfied; lighter footprint than Qt | Smaller, less actively modernized ecosystem than Qt or Dear ImGui (fewer maintained examples/bindings to build on); still a retained-mode widget-tree model with real per-platform quirks to debug (DD5's maintenance cost, though lower than Qt's) | Rejected - a reasonable second choice (passes DD3 unlike Qt), but offers no advantage over Dear ImGui on DD1/DD2/DD4/DD6, and DD5's maintenance cost, while lower than Qt's, is still higher than an immediate-mode UI's for this use case |
| **E - Web-view/Tauri-style UI on a future Rust binding** - speculative | Could reuse modern web UI tooling (React/Svelte/etc.) for layout/theming; Tauri's bundle size is smaller than an Electron-style alternative | **Depends on prospective work that does not exist**: no Rust binding for this project exists today (DD4), and even `smbios_c` - the only C ABI any binding could wrap - does not expose `get_fields()`/`IOutputFormatter` yet (an explicit milestone 0009 non-goal); building this option first requires (1) extending `smbios_c` with a structured-field C API, then (2) writing and maintaining a Rust binding over it, then (3) the Tauri shell itself - three unstarted dependencies deep | Rejected for now - not on technical merit per se, but because it is sequenced behind two pieces of work this project has only ever discussed as future possibilities, never scoped; revisit only after both exist |

## Consequences

### Positive

- A future GUI milestone has a concrete, low-risk technology recommendation
  to scope against, instead of re-litigating toolkit choice from scratch
  once someone picks this up.
- The recommended design reuses milestone 0009's structured-field seam
  (`get_fields()`/`IOutputFormatter`) and milestone 0004's self-registering
  factory exactly as built - no new decode logic, no new registry pattern,
  and no change to `libsmbios`'s public API is required to support it.
- Ruling out the Rust/Tauri path for now (DD4) keeps this ADR honest about
  what actually exists versus what is only discussed in other milestones'
  notes - it does not accidentally commit a future GUI milestone to also
  building a language binding it never asked for.

### Negative

- Dear ImGui's immediate-mode UI will not look or behave like a native
  Windows/macOS/GTK application (no native menu bar, no OS theming, no
  built-in accessibility-API integration) - a real limitation if this tool
  is ever repositioned from "diagnostic utility" to "shipped end-user
  product," at which point this ADR should be revisited rather than quietly
  overridden.
- A GPU-capable rendering backend (SDL2/GLFW plus a graphics API binding)
  becomes a new build/runtime dependency this project does not have today,
  resolved through the same `vcpkg.json` manifest as Boost but still a real
  addition to what `cmake --preset <os>` must provision on a clean checkout.
- This option leaves the `smbios_c`/future-binding path (Alternative E)
  unaddressed; if a Python/Rust/C# binding is ever built for a different
  reason, a second, separate ADR will be needed to decide whether that
  binding should also grow a GUI consumer, rather than assuming this ADR's
  choice extends to it automatically.

## Validation / Rollout

This ADR is exploratory and has no implementation attached to it yet. If and
when a roadmap milestone adopts this decision:

- A minimal spike (one window, one `SMBios` instance, a tree of real
  structures from the current host, `get_fields()` rendered as rows) should
  be built and run on all three CMake presets (`windows`/`linux`/`macos`)
  before any further UI work, to confirm the whole-archive link pattern and
  the chosen windowing/GPU backend both resolve cleanly through
  `vcpkg.json` on each platform - the same bar ADR 0001's own Story 01.1
  rollout held itself to for its skeleton restructuring.
- Success is "the GUI shows the same structures, in the same count, as
  `smbios`'s own `--format text` output for the same live table" - a direct,
  checkable parity test against the existing CLI, not a new source of
  truth.
- If Alternative E's prerequisites (a `smbios_c` structured-field API, a
  Rust binding) are ever separately scoped and built for other reasons,
  revisit this ADR rather than assuming its Decision still holds once those
  exist.

## Links

- **Roadmap story/task:** _(none yet - this ADR is exploratory, ahead of any
  roadmap milestone; see `docs/roadmap/README.md` for the current, fully
  sequential 0001-0010 program this would follow)_
- **Supporting specs:**
  [docs/roadmap/0009-structured-output-formats/plan.md](/docs/roadmap/0009-structured-output-formats/plan.md)
  (the `get_fields()`/`IOutputFormatter` seam this ADR's Decision consumes),
  [docs/roadmap/0007-c-abi-shim-for-cross-language-interop/plan.md](/docs/roadmap/0007-c-abi-shim-for-cross-language-interop/plan.md)
  and its
  [status.md](/docs/roadmap/0007-c-abi-shim-for-cross-language-interop/status.md)
  (why Alternative E is blocked on unstarted work)
- **Diagrams:** _(none needed - no new dependency graph beyond the existing
  `libsmbios`/`smbios` link relationship described in root `CLAUDE.md`)_
