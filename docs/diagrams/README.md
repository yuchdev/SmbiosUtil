# Architecture diagrams

Mermaid diagrams (rendered inline by GitHub) of the SMBIOS SDK, each
grounded in the code and docs it cites. They complement, and do not replace,
[`../architecture/overview.md`](../architecture/overview.md) (runtime data
flow) and [ADR 0001](../adr/0001-per-os-smbios-source-libraries.md)
(library boundaries). Keep each diagram to one idea; if the code changes,
fix the diagram in the same change.

| # | Diagram | Kind | Answers |
|---|---------|------|---------|
| 1 | [Libraries and link dependencies](01-components-libraries.md) | component (flowcharts) | Which targets exist, what links what, how the platform is chosen at CMake time, what each test executable links. |
| 2 | [Core class view](02-core-class-view.md) | component (class) | How `SMBios`, the `ISmbiosSource` chain, `DMIHeader`, `SMBiosEntryFactory`, `AbstractSMBiosEntry::get_fields` and the `IOutputFormatter` registry relate. |
| 3 | [`SMBios` construction](03-smbios-construction.md) | process (sequence, flow) | Source chain walk, per-platform source order, and the `read_smbios_table()` walk. |
| 4 | [CLI run](04-cli-run.md) | process (flow) | `smbios` from argument parsing through `--format` to `render_document()`. |
| 5 | [GUI flow](05-gui-flow.md) | process (sequence) | `build_entry_tree`, the ImGui frame, `display_text` + `TextUnformatted`, and "Copy as" via `render_entry_as`. |
| 6 | [Test and CI flow](06-test-and-ci-flow.md) | process (flow) | `gui-ci` vs `gui-local` tiers and the `build`, `sanitizers` and `gui-build` CI jobs (as-is). |
| 7 | [Build, package and publish pipeline](07-build-package-publish-pipeline.md) | process (flow), layering | Planned (ADR 0005) lint, build/test, package, verify, publish stages, artifact and digest gating, tier table, and the adapter / neutral `CI_*` / `stage.py` / reporter layering for CI-platform portability. |

## Conventions

- Notation: Mermaid in fenced blocks inside `.md` files; no build step.
- Class, function and target names are copied from the sources, never
  invented.
- No secrets, hostnames or absolute local paths.
- Mermaid is not rendered by any local tool in this repository; check a
  changed diagram in the GitHub preview before merging.
