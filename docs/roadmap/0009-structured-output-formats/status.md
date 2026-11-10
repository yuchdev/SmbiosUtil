# Milestone 0009 - Structured Output Formats - Status

Tracks progress against [plan.md](plan.md). Updated as each story lands.

## Current status

| Story | Name                                           | Status         | Tests |
|-------|----------------------------------------------------|----------------|-------|
| 01.0  | Structured field model                             | ✅ Complete (2026-11-10) | 518 unit |
| 02.0  | Renderer registry and built-in formats             | ✅ Complete (2026-11-10) | 602 unit |
| 03.0  | YAML renderer and custom-format extensibility      | ✅ Complete (2026-11-10) | 634 unit, 21 ctest |

**Legend:** ✅ Complete · 🔶 In progress / partial · ⬜ Not started

**Milestone gate (2026-11-10): ✅ PASSED.**
- All three stories passed `/verify-task` on every task and a
  `/pr-review` (feature-reviewer LGTM; security-auditor PASS on 02.0's
  re-check, with 03.0's LOW items SR-6 and SR-7 fixed in `7cd06a6`).
- Full `ctest` passed 21/21 on macOS, with 634 unit cases.
- CI is green on all 5 jobs across Windows, Linux and macOS.
- Real-hardware `--format` validation passed on Linux and Windows CI
  runners and on a macOS machine.
- The doc-sample check and `/link-check` are clean.

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
- **2026-11-10 - The user added examples/documentation scope, which became
  Story 03.0 Task 03; the tests task was renumbered to 04.** This adds
  requirement R7 to [plan.md](plan.md). The spec is
  [03-examples-and-documentation.md](03.0-yaml-renderer-and-custom-format-extensibility/03-examples-and-documentation.md),
  and the exit gate is now
  [04-tests.md](03.0-yaml-renderer-and-custom-format-extensibility/04-tests.md).
  The old `03.0-.../03-tests.md` is a superseded stub awaiting `git rm`.
  No code has been written and the status rows are unchanged. Architect
  decisions, not user-ratified:
  - **Location.** Standalone consumer examples go in a new top-level
    `examples/`, built by default with `SMBIOS_BUILD_EXAMPLES=ON` and linked
    whole-archive like an external consumer. The CSV formatter stays in
    `smbios/examples/`, because it is compiled into the CLI (Task 02, R5).
  - **Deterministic input.** `--read-file` is unimplemented and
    security-gated (see `docs/security/README.md`), and `SMBios` has no
    public buffer or source constructor.
    - The examples therefore read the live machine and *degrade* to an
      empty, well-formed document with exit 0.
    - CTest asserts exit status and well-formedness only.
    - Doc samples come from a fixture built in code with
      `SyntheticTableBuilder` (types 0, 11 and 32; no identifying types).
      Golden files live in `tests/data/output_formats/`. No binary dump is
      checked in.
  - **CI gap found.** The must-pass ctest regex in `ci.yml` does not match
    `smbios_cli_*`, so Task 02's unknown-format tests would never run in
    CI. Task 02 has been amended. Task 03 adds `smbios_example_` to the
    regex.
  - **Hook into Story 02.0.** The README now requires deterministic
    formatter output (no timestamps, host data or CRLF) and Doxygen
    written up front. Task 01's spec is untouched.
  - **Open questions:**
    1. Should a follow-up ADR add an injectable
       `SMBios(std::vector<std::unique_ptr<ISmbiosSource>>)` constructor,
       plus a versioned dump format for `--read-file`/`--dump-file`? Today
       `--dump-file` drops the SMBIOS version. This would let the examples
       take a fixture path, and it would unblock `tests/mock`.
    2. Should a Doxyfile or doc-generation target be added? None exists, so
       Task 03 enforces Doxygen completeness by review only.
    3. `docs/cli-usage.md` says that Linux sysfs reads need no privilege,
       but `/sys/firmware/dmi/tables/DMI` is mode 0400. Task 03 verifies
       this and corrects the doc.

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
  - *Amended 2026-11-10 (security-auditor INFO-6):* folded into the Story 02.0
    specs (README, Tasks 01-03) and the Story 03.0 Task 01/02 specs.
    Renderer stance on LOW-3: **render exactly what `get_fields()` returns,
    and never clamp in a renderer.**
- **Follow-up (LOW-3), entry getters, not renderers:** clamp the
  repeated-record loops in Memory Channel, Firmware Inventory, Event Log
  descriptors, MCHI protocol records and Firmware Language to
  `min(count, records that fit in Length)`. Expose the declared count as
  its own field, and change `render_to_description()` and `get_fields()`
  together so they stay in agreement. Story 02.0 Task 03 case 11's expected
  count moves with it.
- **Former open questions (security-auditor INFO-6). Resolved 2026-11-10 by
  the loop taking the reversible default; revisit any of them on request:**
  - JSON `uint64_t` values stay exact-digit numbers, and the 2^53 limit is
    documented. Switch to strings only if a JavaScript consumer appears,
    since a switch changes the type contract of every integer field.
  - Stripping ESC/C1 controls from `text` / `render_to_description()`
    (CWE-150) is a **post-0009 follow-up**. R3 forbids it inside this
    milestone.
  - Escaping U+2028/U+2029 in JSON is kept. The output is still valid JSON
    and is safe when embedded in JavaScript.
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
- **2026-11-10 - Story 02.0 Task 02 passed verify-task.** Open advisory:
  the XML and JSON writers do not check for duplicate keys at runtime.
  They rely on C1's unique-name invariant, which the 45-type coverage test
  enforces. The threat model says serializers "should still assert".
  Story 02.0 Task 03's `FieldNamesAreFirmwareIndependent` coverage should
  either add a debug-mode assertion or accept the risk explicitly.
- **2026-11-10 - Duplicate-key advisory resolved: risk accepted.** No
  runtime assert is added to the writers. Field names are library
  constants, never firmware data. Two tests enforce uniqueness:
  - `is_well_formed_json` rejects duplicate keys after decoding escapes.
  - `FieldNamesAreFirmwareIndependent` checks that names stay identical and
    unique across all 45 types when every string is hostile.

  Revisit if a formatter ever derives names from data.

### Story 02.0 - Renderer registry and built-in formats (✅ 2026-11-10)

**Delivered**
- Contract C2: `IOutputFormatter` (`render`, `format_name`, and the
  `begin_document`/`entry_separator`/`end_document` framing hooks), the
  Meyers-singleton registry and `REGISTER_OUTPUT_FORMATTER` (`f3447b2`).
- `text`, `xml` and `json` formatters plus the shared
  `formatter_escaping` helpers: strict UTF-8 with U+FFFD replacement,
  XML/JSON/YAML escapers and lowercase `hex_encode` (`edc344b`).
  - `text` is byte-identical to `render_to_description()` (R3).
  - `xml` and `json` use only `get_type()` and `get_fields()` (R4).
- Structured-output validators and the hostile-document security cases
  (`92a768e`).
- Review fixes (`c5bd074`):
  - JSON escapes C1 controls U+007F..U+009F (SR-1); YAML escapes U+FEFF
    (SR-2); registry documented as static-init only (SR-3).
  - `render_document()` (nulls skipped, no stray separators) and
    `field_value_to_string()` are now public API.
  - Both registration macros accept namespace-qualified class names
    (`__COUNTER__` via `registration_name.hpp`); `REGISTER_SMBIOS_ENTRY`
    had the same flaw.
  - An empty `format_name()` throws `std::invalid_argument`.

**Tests / gate**
- `smbios_unit_test`: 602/602 cases (518 at Story 01.0 close). Full
  `ctest` on macOS: 9/9.
- `/verify-task`: PASS on Tasks 01, 02 and 03.
- `/pr-review` round 1: feature-reviewer REQUEST_CHANGES, security-auditor
  CONCERNS (SR-1..SR-3). Round 2, on `c5bd074`: feature-reviewer LGTM,
  security-auditor PASS.
- The local `build/macos-asan` tree failed to link during the security
  re-check; this was not investigated, and CI's sanitizer jobs on this push
  are the authoritative signal.

**Reconciliation note**
- No task spec was superseded. `render_document()` and
  `field_value_to_string()` go beyond the Task 01/02 specs as review-driven
  additions; Story 03.0 (YAML, CSV, CLI wiring) should build on them rather
  than re-implementing document framing.

**Carried forward**
- CWE-150 terminal escapes in `text`: post-0009 follow-up, see Notes &
  decisions. Recommend opening a tracked issue at milestone close.
- SR-4 (XML normalises CR to LF) accepted as is.

- **2026-11-10 - Story 03.0 Task 01 (YAML renderer) passed verify-task**
  (`ff606d3`). A zero-entry document is `---\n\n`, which loads as null,
  not `[]`: the framing hooks cannot know the entry count. The spec is
  silent on it; the header documents "null means no entries". The PyYAML
  6.0.3 round-trip was run once, dev-only (trap strings, uint64 max, C1
  and U+2028 escapes, the 17-entry hostile corpus all passed); record the
  command in the milestone PR description.
- **2026-11-10 - Story 03.0 Task 02 (CSV example, `--format`) passed
  verify-task** (`0d7d781`, `ef62ca0`, `184b692`). Choices the spec left
  open, documented in `smbios/examples/csv_formatter.cpp`'s header:
  LF row endings (not RFC 4180 CRLF, matching the other formats); an
  entry with no fields still emits one `Type,,` row; the formula-injection
  guard makes values starting with `-` show a visible `'`. CI's must-pass
  regex now includes `smbios_cli_unknown_format*`; `smbios_cli_format_csv`
  (reads real hardware) is warning-only.
- **2026-11-10 - Story 03.0 Task 03 (examples and documentation, R7)
  complete.** Code: `3174884`, `8d2ba4f`, `38bfee9`, `ccfd252`. Docs:
  `671b0f9`, `8ca1acc`, `a8496d8`, `dcea9c1`, `c730694`. verify-task first
  returned FAIL (the `docs/test/test-subsystem.md` and
  `docs/dev/cpp_coding_standard.md` rows were missing); both were added in
  `c730694`. Logged deviations:
  - `examples/read_fields/main.cpp` is 76 non-comment lines, against
    "about 60".
  - `format_table` also includes `abstract_smbios_entry.hpp`, because
    `std::unique_ptr` needs the complete type.
  - `FixtureCoversEveryFieldValueAlternative` was written by cpp-expert;
    testing-expert reviews it in Task 04.
  - The Linux sysfs table mode (0400, root-only) is still not verified on
    real Linux. The docs hedge ("typically") and say so. Follow-up: add a
    one-line `ls -l /sys/firmware/dmi/tables` probe to the ubuntu CI job.
  - Fixed along the way: the root `README.md` and `CLAUDE.md` linked
    `smbios_c/include/smbios/smbios.hpp`; the header is `smbios.h`.
- **2026-11-10 - Story 03.0 Task 04 (milestone exit gate) complete.**
  - **Privacy test strengthened** (`95ffc71`). The test now checks the raw
    fixture bytes for SMBIOS types 1/2/3/17, requires an `Example` marker in
    every DMI string, and rejects any field name containing serial, uuid,
    asset, part number or sku. Each of the three checks was confirmed to
    fail on a deliberately broken fixture.
  - **Real hardware, all five formats, through the actual CLI** (`927ca5f`
    adds a CI step; run 42821726781). A malformed document fails the job.
    - Linux `ubuntu-latest` (sudo) and Windows `windows-latest`: 14
      structures in xml/json/yaml, 142 csv rows.
    - macOS dev machine (IOKit): 26 structures; the YAML parse equals the
      JSON, and the CSV row count equals the JSON field count.
    - The macOS CI runner exposes no table, so there the step only warns
      on the empty, well-formed documents.
  - **`text` output is unchanged.** It is byte-identical to `master`'s CLI
    on the dev Mac, and no-flag output equals `--format text` on every CI
    runner.
  - **Linux sysfs mode verified.** `DMI` and `smbios_entry_point` are
    `-r-------- root root` on the runner. `docs/cli-usage.md` now states
    this as fact instead of hedging.
  - **Doc correction.** `docs/cli-usage.md` claimed a table-read failure
    exits non-zero. In fact, when no source succeeds, `SMBios` has an empty
    table and the CLI exits 0 with an empty result; this behaviour predates
    0009. The doc now says so.
  - **Follow-ups:**
    - Consider a stderr diagnostic and a non-zero exit when no source
      succeeds. This is a behaviour change; it needs its own decision.
    - Make zero-entry YAML a real empty sequence: have `render_document()`
      or a new `empty_document()` hook emit `[]`. CI runners showed that
      empty tables are common.
    - `--read-file` is still a `TODO` in `smbios/src/main.cpp`. The threat
      model's attacker-controlled dump path is therefore latent: it is not
      reachable today.

### Story 03.0 - YAML renderer and custom-format extensibility (✅ 2026-11-10)

**Delivered**
- `yaml` formatter (`ff606d3`). Every key and string is double-quoted with
  full YAML 1.2 escapes, including U+FEFF. It never emits anchors, tags or
  aliases.
- CSV example formatter, `smbios/examples/csv_formatter.cpp` (`0d7d781`).
  - It is compiled into the CLI as an OBJECT library. R5 holds: the only
    edit to existing build files is one `add_subdirectory` line, plus
    test-side wiring that the spec allows.
  - Cells starting with `= + - @` TAB or CR get a `'` prefix (CWE-1236).
  - Terminal controls (C0 except TAB/CR/LF, DEL, C1) become U+FFFD (SR-7,
    `7cd06a6`).
- `--format`/`-f` on the `smbios` CLI (`ef62ca0`).
  - The format is validated before `SMBios` is constructed. An unknown name
    lists the registered formats and exits 1 (R6).
  - `text` stays byte-identical to the pre-milestone output.
  - Other formats go through `render_document()`.
- Examples and documentation (R7):
  - `examples/format_table` and `examples/read_fields`, plus the validator
    `examples/scripts/check_structured_output.py`.
  - A synthetic doc-sample fixture and golden files in
    `tests/data/output_formats/`, checked by `scripts/check_doc_samples.py`.
  - New guides: `docs/output-formats.md` and
    `docs/dev/custom-output-formatter.md`.
  - Updates to `cli-usage.md`, `architecture/overview.md`, `README.md`,
    `CLAUDE.md`, `test-subsystem.md` and `cpp_coding_standard.md`.
  - A Doxygen gap-fill pass.
- CI:
  - The must-pass regexes now include `smbios_cli_unknown_format*` and
    `smbios_example_*`.
  - A Linux doc-sample check step.
  - A real-hardware `--format` validation step in the `build` job on all
    three OSes (`927ca5f`).

**Tests / gate**
- `smbios_unit_test`: 634/634. Full `ctest` on macOS: 21/21.
- CI run 42821726781: green on all 5 jobs.
  - Linux and Windows runners: 14 structures in xml/json/yaml, 142 csv rows.
  - The macOS runner exposes no table, so that step only warns.
  - On the dev Mac, 26 structures, and `text` is byte-identical to `master`.
- `/verify-task`:
  - Tasks 01, 02 and 04: PASS.
  - Task 03: FAIL at first, because two doc rows were missing; it passed
    after `c730694`.
- `/pr-review`: feature-reviewer LGTM. security-auditor CONCERNS: LOW SR-6
  (live table in CTest failure output, CWE-532) and SR-7 (CSV terminal
  controls, CWE-150), both fixed in `7cd06a6`. SR-9 hardening (XML
  DOCTYPE/ENTITY rejected by the validator) was also applied. INFO SR-8 and
  SR-10 are accepted.

**Reconciliation note**
- The original `03-tests.md` became `04-tests.md` when the user added R7
  scope as Task 03. Every task spec is *consumed*; none is superseded.

**Carried forward (post-0009 follow-ups)**
- **CLI exit codes.** If no source succeeds, the CLI exits 0 with an empty
  result. An exception, or a bad Boost.Program_options option, also exits
  0. Structured-format consumers would benefit from a non-zero exit. This
  is a behaviour change, so it needs its own decision.
- **Zero-entry YAML** loads as null. Emit `[]` from `render_document()` or
  a new `empty_document()` hook.
- **`legacy_text` keys off `format_name() == "text"`.** Consider an
  explicit hook.
- **`--read-file` is still a TODO.** The attacker-controlled-dump surface
  is latent.
- **The whole-archive link block is duplicated** in three CMake files.
  Extract a helper.
- **CWE-150 in `text` output** (from Story 02.0) still needs a tracked
  issue.
- **Test gaps:**
  - an end-to-end CLI `text` byte-identity test against a fixture;
  - a `--format yaml --dump-file` legacy-output test.
