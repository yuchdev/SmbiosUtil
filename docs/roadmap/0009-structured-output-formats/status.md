# Milestone 0009 - Structured Output Formats - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                           | Status         | Tests |
|-------|----------------------------------------------------|----------------|-------|
| 01.0  | Structured field model                             | ⬜ Not started | -     |
| 02.0  | Renderer registry and built-in formats             | ⬜ Not started | -     |
| 03.0  | YAML renderer and custom-format extensibility      | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Added at the user's request: the current `smbios_util` has exactly one
  output path (free-form text to stdout), confirmed by reading
  `main.cpp` directly - no existing milestone (0001-0008) assures
  structured output, so this is genuinely new scope, not a gap in an
  existing one.
- **JSON library decision, open for Story 02.0:** candidates are
  Boost.JSON (already a transitive vcpkg dependency via Boost, per
  milestone 0001 - confirm the pinned vcpkg baseline's Boost version is
  ≥ 1.75, Boost.JSON's minimum) vs. a new dependency (nlohmann/json) vs.
  Boost.PropertyTree (already used nowhere in this codebase yet, but part
  of Boost, with built-in `write_json`/`write_xml` - known to handle
  arrays/type-fidelity awkwardly). Recorded here rather than decided
  unilaterally in `plan.md`, since it's a real tradeoff Story 02.0's
  implementer should weigh with the actual pinned Boost version in hand.
- **YAML library decision, open for Story 03.0:** candidates are a new
  `yaml-cpp` vcpkg dependency vs. a hand-rolled minimal flat-YAML emitter
  (justified by the data shape: `NamedField` lists are flat key/value,
  occasionally a list of strings for a bitmask property set - not deeply
  nested, so YAML's full feature set is not needed). Leaning
  hand-rolled to avoid a new dependency, but not finalized here.
