# Milestone 0009 - Structured Output Formats - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                           | Status         | Tests |
|-------|----------------------------------------------------|----------------|-------|
| 01.0  | Structured field model                             | ✅ Complete (2026-11-10) | 518 unit |
| 02.0  | Renderer registry and built-in formats             | ⬜ Not started | -     |
| 03.0  | YAML renderer and custom-format extensibility      | ⬜ Not started | -     |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

## Notes & decisions

- Added at the user's request: the CLI (then `smbios_util`, now the `smbios` target) has exactly one
  output path (free-form text to stdout), confirmed by reading
  `main.cpp` directly - no existing milestone (0001-0008) assures
  structured output, so this is genuinely new scope, not a gap in an
  existing one.
- **2026-11-10 - JSON library decision: RESOLVED, hand-rolled (user-ratified).**
  `JsonFormatter` uses a hand-rolled emitter. No new dependency is added:
  no Boost.JSON, no nlohmann/json, no Boost.PropertyTree, and `vcpkg.json`
  is untouched. For the record, the pinned Boost is 1.92, so Boost.JSON's
  ≥ 1.75 minimum was met. It was still declined, for dependency hygiene
  and so that all three structured writers stay symmetrical. Story 02.0
  Task 02 no longer carries a "decide the library" step. This replaces
  the earlier "open for Story 02.0" bullet.
- **2026-11-10 - YAML library decision: RESOLVED, hand-rolled flat
  emitter (user-ratified).** There is no `yaml-cpp`, not even in a test
  binary. Every string scalar is double-quoted, including field-name keys,
  to avoid YAML implicit-typing traps (`NO`, `1.10`, `0x0A`, `~`). Only
  the fixed structural keys `type`/`fields` and the `uint64_t`/`bool`
  values are plain. This replaces the earlier "open for Story 03.0"
  bullet, and also the old spec's "quote only when ambiguous" rule.
  Shared escaping helpers (`formatter_escaping.hpp`: XML/JSON/YAML
  escapers plus `hex_encode`) are used by all writers, per the user's
  encouragement.
- **2026-11-10 - `get_fields()` is pure virtual on all entry types
  (user-ratified; C1 as written).** This covers all 45 types. During
  Story 01.0 only, a TEMPORARY non-pure default is allowed (see the
  Task 02 split below). It is gone at story exit.
- **2026-11-10 - Spec maintenance: milestone re-based onto the current
  codebase (no code written; status rows unchanged).** Gaps found and how
  each was handled:
  1. **Stale paths.**
     - The specs said `smbios/include/smbios/...`, `smbios/src/...` and
       `smbios_util/...`. They are re-pathed to the core library
       `libsmbios`: public headers under `libsmbios/include/libsmbios/`
       (included as `<libsmbios/...>`), sources in `libsmbios/src/`.
       Formatter headers go in a new `include/libsmbios/formatters/`, and
       their sources stay flat in `src/`, because the
       `libsmbios/src/CMakeLists.txt` glob is non-recursive. This mirrors
       `tables/`.
     - CLI references now point to the `smbios` target:
       `smbios/src/main.cpp`, `smbios/src/command_line_parser.cpp` and
       `smbios/include/smbios/command_line_parser.hpp`. The CSV example
       moves to `smbios/examples/`.
     - `FieldValue`/`NamedField` are consolidated into one header,
       `field_value.hpp`.
     - The stale `main.cpp:84-92` reference is corrected to
       `main.cpp:115-124`.
  2. **Scope is 45 types, not 11.** "Milestone 0004's eleven" is replaced
     by the 45 concrete `AbstractSMBiosEntry` subclasses that milestone
     0010 registered. That is exactly one `REGISTER_SMBIOS_ENTRY` each in
     `libsmbios/src/`, including `InactiveEntry` and
     `StringPropertyEntry`. There is no intermediate base class. Two test
     doubles also subclass it, plus a third that Story 01.0 Task 01 adds:
     `FakeTestEntry` (`tests/unit/smbios_entry_factory_test.cpp`) and
     `TestEntry` (`tests/unit/dmi_string_parsing_test.cpp`). The pure
     flip must update them. `smbios_c` does not subclass
     `AbstractSMBiosEntry` (consumer only), so it is unaffected.
  3. **Story 01.0 Task 02 is split into green batches.**
     - Task 01 adds `get_fields()` as a TEMPORARY non-pure virtual that
       returns `{}`.
     - Tasks 02-05 migrate 12/10/12/11 types by family: platform and
       firmware; processor and memory; slots, devices and power; probes
       and management.
     - Task 05 flips `get_fields()` to `= 0` in the same commit as its
       batch, and adds a 45-type coverage test.
     - The exit gate is renumbered to 06. The old
       `02-migrate-entries-to-structured-fields.md` and `03-tests.md` are
       superseded stubs awaiting `git rm`.
  4. **Library decisions** are resolved as recorded above.
  5. **Linking.**
     - `REGISTER_OUTPUT_FORMATTER` initializers in `libsmbios` run
       automatically in `smbios`, `smbios_c` and `tests/c_abi`, all of
       which force-load the whole archive.
     - `smbios_unit_test` does not force-load, so formatters define their
       members out of line and the unit tests reference the formatter
       classes. This is the same mechanism the entry tests already use.
     - The CSV example is compiled straight into `smbios`, so it needs no
       whole-archive flag.
  6. **Contract additions made during the re-base (Architect, not
     user-ratified).**
     - C2 gains `registered_output_formatter_names()`. R6's error listing
       and `--help` need it, and C2 previously had no enumeration API.
     - C1 gains the invariant that field names are unique within one
       entry, using 1-based index suffixes, so JSON/YAML mappings have no
       duplicate keys.
     - Escapers replace invalid UTF-8 with U+FFFD, because DMI strings
       are not guaranteed to be UTF-8.
     - `--format bogus` must exit non-zero via `stderr`, not via
       `usage()`, which exits 0 today.
  7. **R3-R6 are kept intact**, only re-pathed. R5's "zero changes" is
     made precise: no existing `libsmbios`/`smbios` *source* file is
     edited for the CSV example. The only edit is one
     `add_subdirectory(examples)` build-wiring line in
     `smbios/CMakeLists.txt`.
- **RATIFIED 2026-11-10 (user) - multi-entry document framing: option
  (a).** `IOutputFormatter` gains non-pure `begin_document()`,
  `entry_separator()` and `end_document()` hooks that return an empty string
  by default. This is additive to C2: `render(entry)` is unchanged. The CLI
  prints the `DMI version`/`Table size`/`Header ID` preamble only for
  `text`, so `text` stays byte-identical (R3). Option (b), JSON Lines, and
  option (c), `render_document(range)`, were rejected. Story 02.0 Task 01
  implements this; it no longer has to resolve it first.
- **RATIFIED 2026-11-10 (user) - the item 6 contract additions are
  accepted**: `registered_output_formatter_names()`, unique field names
  with 1-based suffixes, U+FFFD replacement for invalid UTF-8, and a
  non-zero exit for an unknown `--format`.
- **RATIFIED 2026-11-10 (user) - `--format` CLI smoke tests** are plain
  CTest `add_test` entries in `tests/integration/CMakeLists.txt`,
  classified as Integration. Add a note to `docs/dev/cpp_coding_standard.md`'s
  Testing section that CTest-level CLI smoke tests belong to that category.
- **2026-11-10 - Story 01.0 Task 03 (batch 2) disposition notes.**
  - `"Processor-Specific Data"` drops the text label's dynamic
    `(N bytes)` suffix, so the key stays stable. The byte count can be
    derived from the raw-bytes value.
  - Cache SRAM bits that occur in both groups are named
    `"<bit> (Supported)"` and `"<bit> (Installed)"`, instead of numeric
    suffixes.
  - **Follow-up bug, out of scope here because R3 requires byte-identical
    text:** `MemoryDeviceEntry::get_data_width_string()`
    (`libsmbios/src/memory_device_entry.cpp:429`) checks the data width for
    special values but then prints `get_total_width()`. This is a
    copy-paste defect. The structured fields deliberately mirror it, so
    text and fields stay in agreement. The fix should be a separate
    change that corrects both outputs together and adds a unit test.
- **2026-11-10 - Story 01.0 Task 04 (batch 3): verify-task PARTIAL, resolved
  by amending the spec.** Event-log descriptors keep the text's 0-based
  numbering, and `SystemSlots` peer groups are out of scope because no
  getter exists (R1). Existing getter quirks are mirrored, not fixed. Each
  is a candidate for a follow-up:
  - `PortConnection` getters `assert(false)` on unmapped codes.
  - `PortableBattery` `uses_sbds_*()` returns true on a too-short
    structure.
  - `SystemPowerControls` decodes BCD without validating it (0xFF → 165).
  - `SystemEventLog` Access Method Address prints as decimal, not hex.
  - `TpmDevice` firmware versions print raw, with no per-spec-version
    decode.
  - A local macOS ASan run aborts with a Boost.Test "container-overflow" at
    test registration. It is clean with `detect_container_overflow=0`.
    This is likely an uninstrumented-Boost false positive; confirm against
    CI's sanitizer job at story close.
- **2026-11-10 - Story 01.0 Task 05 (batch 4) passed verify-task, with spec
  amendments.**
  - `BootIntegrityServicesEntry` yields no fields, like `InactiveEntry`,
    because its text output is only the header and a fixed sentence.
  - The coverage test uses a 0x60 formatted area filled with `0x02`, at
    SMBIOS 3.9, rather than a minimal structure.
  - Structured output is a *text-parity* view: fields that
    `render_to_description()` never prints are not emitted (OEM-defined
    probe data, IPMI modifier bits, host-interface specific bytes). A
    complete decode would be new scope.
  - Its 45-row coverage table is hand-maintained, because the factory has no
    enumeration API. Task 06 should close this drift gap.
  - More mirrored quirks for follow-up:
    - Probe and threshold values are decoded as unsigned `uint16_t`, so a
      negative temperature prints as 65526. dmidecode treats these values
      as signed.
    - The IPMI spec revision is BCD with no validation and no dot.
    - Handles and addresses are printed in decimal in the
      management-device, component and IPMI structures.
    - When a host-interface record count overstates the buffer, the
      missing records are filled with defaults.

## Story details

### Story 01.0 - Structured field model (✅ 2026-11-10)

**Delivered**
- `libsmbios/include/libsmbios/field_value.hpp`:
  - `FieldValue` is `std::variant<std::string, uint64_t, bool, std::vector<uint8_t>>`.
  - `NamedField` is `{name, value}`.
- `AbstractSMBiosEntry`:
  - `get_fields() const = 0`, pure virtual. The compiler now makes every
    current and future entry type implement it.
  - A protected `bitset_to_fields()` helper.
- `get_fields()` is implemented on all 45 registered entry types, in four
  batches (`07568f5`, `2373683`, `bac07b9`, `b27a4da`).
  - Each one mirrors its own `render_to_description()`: same fields, same
    guards, same order.
  - Each one is built only from existing getters (R1).
  - Text output is byte-identical (R3).
  - `InactiveEntry` and `BootIntegrityServicesEntry` return an empty list
    on purpose.
- Small private predicate helpers are shared between the text and field
  paths. They are logically identical to the code they replaced, and
  feature-reviewer confirmed this.
  - Baseboard: `feature_flags_present`.
  - Processor: `characteristics_are_none`.
  - Cache: `sram_types_are_none`.
  - Slots: `characteristics_sentinel`.
- **Security fix (`30cb8b3`)**, from security-auditor HIGH-1 (CWE-835/770):
  - The OEM Strings and System Configuration Options loops used a
    `uint8_t` counter. With a count of 0xFF the counter wrapped and the
    loop never ended. This affected both `render_to_description()` (a
    defect already on `master`) and `get_fields()`.
  - The counters are now `unsigned`.
  - Regression tests run under `timeout(10)`.
- **Fuzzer hardening (`30cb8b3`)**, from MEDIUM-2:
  - The fuzzer now creates each entry and calls `render_to_description()`
    and `get_fields()` on it.
  - `libsmbios` is now instrumented under `SMBIOS_BUILD_FUZZERS`.
  - A 60 s Release run was clean: 208,868 executions.
- Threat model: `docs/security/2026-11-10-structured-field-model.md`.

**Tests / gate**
- `smbios_unit_test`: 518/518 cases (415 before this story), 9485
  assertions.
  - `tests/unit/field_value_test.cpp`
  - `tests/unit/field_assertions.hpp`, which provides the ordered-field and
    unique-name checks
  - `tests/unit/get_fields_coverage_test.cpp`, which covers all 45 types
    through the factory
  - Per-type `GetFieldsMatchesDecodedValues`, `HonoursLengthTiers` and
    `ExpandsBitmask` cases
- Full `ctest` on macOS: 9/9.
- CI run 42793526301 on `a1828bd`: green on all 5 jobs, all three OSes.
- `/verify-task`: PASS on Tasks 01, 02, 03 and 05. Task 04 was PARTIAL and
  resolved by amending the spec.
- `/pr-review`:
  - feature-reviewer: LGTM.
  - security-auditor: CONCERNS, with the HIGH and MEDIUM items fixed in
    `30cb8b3`. The LOW items are carried forward, see below.

**Reconciliation note**
- The original `02-migrate-entries-to-structured-fields.md` and `03-tests.md`
  were *superseded* by Tasks 02-05 (four batches) and 06. They were deleted
  during Phase R.
- The Task 04 and Task 05 specs were *amended in place*. The amendments are
  dated in each file.

**Carried forward**
- **To Story 02.0's spec**, from security-auditor INFO-6 and LOW-3. The
  serializers must:
  - replace invalid UTF-8 with U+FFFD;
  - strip XML-illegal control characters;
  - use double-quoted YAML;
  - neutralise CSV formula injection;
  - account for the JSON 2^53 integer precision limit;
  - cap byte fields.

  They must also decide whether to clamp the repeated-record counts that are
  not checked against structure length.
- **Non-blocking review suggestions:**
  - Add a shared `hex_string()` helper to remove the duplicated hex
    formatting.
  - Add a 0xFF-fill uniqueness variant for the bitmask types.
  - Add a text-vs-fields label-order parity check.
  - Add an enumeration API for the factory registry, so the hand-maintained
    45-row coverage table cannot drift.
- **Follow-up defects**, existing getter quirks that are mirrored rather
  than fixed. They are listed in the Task 03/04/05 notes above. The
  Debug-build `assert(false)` getters (LOW-4) also stop Debug fuzzing early.
