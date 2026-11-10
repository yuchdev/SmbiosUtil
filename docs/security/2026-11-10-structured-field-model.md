# Threat Model - Structured field model (milestone 0009 Story 01.0) - 2026-11-10

Scope: `git diff master...milestone-0009`, excluding `docs/roadmap/**`. Code commits
`b5489e9`, `07568f5`, `2373683`, `bac07b9`, `b27a4da`. This adds
`smbios::FieldValue` / `smbios::NamedField` (`libsmbios/include/libsmbios/field_value.hpp`),
`AbstractSMBiosEntry::bitset_to_fields<T>()` and a pure-virtual `get_fields()`
(`libsmbios/include/libsmbios/abstract_smbios_entry.hpp:39,73`), and one `get_fields()`
override per entry type (45 types) in `libsmbios/src/*_entry.cpp`.

## Assets & trust boundaries

- **Untrusted input:** the raw SMBIOS table bytes. They come from firmware or a
  hypervisor (sysfs, IOKit, `GetSystemFirmwareTable`, `/dev/mem`), or from a
  user-supplied dump file (`smbios --read-file`, `smbios/src/command_line_parser.cpp:22`).
  A dump file shared in a bug report is fully attacker-controlled.
- **Boundary 1 (existing, unchanged):** the table walk `SMBios::read_smbios_table()`
  (`libsmbios/src/smbios.cpp:118-147`) rejects `length < 4` and any structure whose
  declared length runs past `table_end`. So `header.data .. header.data + length`
  (length <= 255) is always in-bounds. `parse_dmi_strings()`
  (`libsmbios/src/abstract_smbios_entry.cpp:17-33`) stops at `table_end` and drops an
  unterminated trailing string.
- **Boundary 2 (new):** `get_fields()` turns that bounded view into owned
  `NamedField` values. Every field *name* is a library-controlled constant (a literal,
  a constant map label or an integer suffix). No firmware byte ever reaches a key.
  Firmware data reaches only the *values*: `std::string` (DMI strings and decoded
  text), `uint64_t`, `bool` and `std::vector<uint8_t>`.
- **Boundary 3 (future, Story 02.0/03.0):** `NamedField` values go to the JSON, XML,
  YAML and CSV serializers, then to files, stdout and the `smbios_c` consumers.
- **Secrets/PII:** there are no credentials. Serial numbers, UUIDs and asset tags are
  host identifiers (low-sensitivity PII) and reach output by design.

## Review of the requested checks

| Check | Result |
|---|---|
| OOB in Group Associations items | Safe. The count comes from `(Length-5)/3` and each `_at()` re-checks `get_entry_size()` (`group_associations_entry.cpp:85-113`). |
| OOB in event-log descriptors | Safe. `get_log_type_descriptor_at()` bounds-checks against length and rejects `descriptor_length > 4` (`system_event_log_entry.cpp:181-195`). The `uint8_t index < count` loop terminates. |
| OOB in Memory Channel devices | Safe. Each `_at()` checks the index against the count and the offset against length (`memory_channel_entry.cpp:103-124`). |
| OOB in Additional Information records | Safe. The offsets are pre-validated in `cache_entry_offsets()`, so each value slice fits inside length (`additional_information_entry.cpp:55-72,143-162`). |
| OOB in MCHI protocol records | Safe. The record walk checks each step against length and `data_length` is `uint8_t` (`management_controller_host_interface_entry.cpp:124-160`). The cost is O(n^2) for n <= 255, which is negligible. |
| OOB in firmware-inventory handles | Safe. The index and the offset are both checked (`firmware_inventory_information_entry.cpp:250-258`). |
| OOB in OEM strings and config options | No OOB, because `dmi_string()` is bounds-checked. **But the loop never terminates (see HIGH-1).** |
| Unbounded allocation from a count field | Yes, through HIGH-1 only. Every other loop is capped at 255 by a `uint8_t` count or by `(255-5)/3`. |
| `assert(false)` newly reachable | **None newly reachable.** The six `assert(false)` sites (`system_information_entry.cpp:108`, `memory_device_entry.cpp:182,230`, `port_connection_entry.cpp:144,160,176`) were already reachable from `render_to_description()` on master. `get_fields()` calls the same getters. In NDEBUG builds the fallback key is present in each map, so the `(*map.find(x)).second` dereferences stay defined. Debug builds abort on out-of-spec firmware bytes (pre-existing, see LOW-4). |
| Raw-byte lengths | Correct. `get_value_bytes_at` uses `Entry Length - 5` after validation. `get_processor_specific_data` (`processor_additional_information_entry.cpp:111-124`) checks `0x06 + block_length` against length. `get_status_data` (`system_boot_information_entry.cpp:75-83`) uses `[0x0B, length)`. Each field is at most 255 bytes. |
| `dmi_string` index misuse | None. The index comes from a firmware byte or a loop counter, and `dmi_string()` returns "Bad index" when it is out of range (`abstract_smbios_entry.cpp:35-41`). Index 0 maps to "Not Specified". |
| `bitset_to_fields<T>` | Terminates for every unsigned `T`. The shift wraps to 0 after the last bit (for `uint8_t`/`uint16_t` the promoted shift is truncated on assignment). |

## Findings

### [HIGH] HIGH-1 Non-terminating loop in OEM Strings / System Configuration Options when count == 255 (unbounded memory growth)
- **Vector / evidence:**
  - `libsmbios/src/oem_strings_entry.cpp:61` and `libsmbios/src/system_configuration_options_entry.cpp:61` (new `get_fields()`).
  - The same pattern was already in `render_to_description()` at line 36 of both files on master. `get_fields()` copies it.
  - The loop is `for (uint8_t index = 1; index <= count; ++index)`. When the firmware Count byte (offset 0x04) is `0xFF`, `++index` wraps from 255 to 0 and `index <= 255` stays true forever.
  - Each pass appends a `NamedField` with a heap-allocated name and value, so the vector grows until the allocator fails. The `render_to_description()` copy grows a `stringstream` the same way.
  - Confirmed with a standalone reproducer of the loop shape (no termination after 100,000 iterations).
  - 255 is a spec-legal count, so even well-formed firmware can trigger this, not just a crafted `--read-file` dump.
- **Impact:**
  - The process hangs, then runs out of memory and aborts. This hits the CLI, every `smbios_c` consumer, and every future Story 02.0 serializer, since they all call `get_fields()` on every entry.
  - CWE-835 (infinite loop), CWE-770 / CWE-400 (uncontrolled resource consumption). OWASP: A04 Insecure Design, DoS category.
  - There is no memory corruption, so this is not CRITICAL.
- **Mitigation (to `cpp-expert`):**
  - Change the loop variable to `unsigned`/`size_t` in all four loops (two `get_fields()`, two `render_to_description()`).
  - Optionally also cap the loop at the number of strings actually present, which needs a `dmi_strings_.size() - 1` accessor. That avoids emitting up to 255 "Bad index" values.
- **Regression tests (to `testing-expert`):** add a unit case per entry with Count = 0xFF that asserts `get_fields().size() == 256` and that `render_to_description()` returns. Run it under a ctest `TIMEOUT`.

### [MEDIUM] MEDIUM-2 Fuzzer does not exercise entry decoding / get_fields()
- **Vector / evidence:**
  - `tests/fuzz/read_smbios_table_fuzzer.cpp` only drives `read_smbios_table()` through `SmbiosTableWalkTestAccess`.
  - It never calls `SMBiosEntryFactory::create()`, `render_to_description()` or `get_fields()`.
  - HIGH-1 is exactly the kind of bug that per-entry fuzzing finds immediately. The 45 decoders and all of their `_at()` index math have no fuzz coverage.
- **Impact:** decoder-level memory-safety and DoS bugs reach the serializers in Story 02.0 unchecked.
- **Mitigation:**
  - Extend the harness so that for each walked header it calls `factory.create()`, then `get_fields()` and `render_to_description()`.
  - Build it with ASan/UBSan and NDEBUG (see LOW-4). libFuzzer `-timeout=` will catch hangs.

### [LOW] LOW-3 Firmware count fields not bounded by Length produce fabricated zero-valued records
- **Vector / evidence:**
  - Memory Channel `get_device_count()` (`memory_channel_entry.cpp:132`).
  - Firmware Inventory `get_associated_component_count()` (`firmware_inventory_information_entry.cpp:279`).
  - Event Log `get_supported_descriptor_count()` (`system_event_log_entry.cpp:237`).
  - MCHI `get_protocol_record_count()` (`management_controller_host_interface_entry.cpp:170`).
  - Firmware Language `get_installable_language_count()`.
  - When the declared count exceeds what Length holds, the out-of-range `_at()` calls return `0` / "Not Specified" / an empty record. Each call is memory-safe, but `get_fields()` emits up to 255 fake fields per entry, such as "Handle 7: 0x0".
- **Impact:** integrity, not memory safety. Machine-readable output from Story 02.0 will present invented handle-0 references as real data, and consumers that follow handles may misattribute data (CWE-1284, improper validation of a specified quantity). The output is bounded at about 2x255 fields per entry.
- **Mitigation:** decide in the Story 02.0 contract whether to clamp the loop to `min(count, records that fit in Length)` and expose the declared count separately, or to keep text parity. Document the choice.

### [LOW] LOW-4 Debug-build `assert(false)` on firmware-controlled enum bytes (pre-existing, inherited)
- **Vector / evidence:**
  - `system_information_entry.cpp:108`
  - `memory_device_entry.cpp:182,230`
  - `port_connection_entry.cpp:144,160,176`
  - An out-of-spec enum byte aborts a non-NDEBUG build. `get_fields()` reaches these sites through the same getters `render_to_description()` already used, so the exposure is unchanged.
- **Impact:** DoS in debug builds and in any fuzz build without NDEBUG (CWE-617, reachable assertion). Release builds fall back safely because the fallback keys exist in each map.
- **Mitigation:** replace with the "OutOfSpec"/"Reserved" fallback without asserting, as `cache_information_entry.cpp` already does. Not a blocker for this story.

### [LOW] LOW-5 `FieldValue` variant can silently bind a `const char*` to `bool` on older standard libraries
- **Vector / evidence:**
  - `field_value.hpp:17` is `std::variant<std::string, uint64_t, bool, std::vector<uint8_t>>`.
  - Under C++20 with P0608/P1957 (current libc++, libstdc++ 10+ and MSVC 19.2x+), `NamedField{"x", "literal"}` selects `std::string`. Pre-P1957 implementations select `bool` (value `true`).
  - The current diff always wraps literals in `std::string(...)`, so there is no live instance today.
- **Impact:** a future edit could silently turn a string value into `true` in serialized output. This is a robustness issue, not an exploitable one.
- **Mitigation:** add a `NamedField` constructor overload that takes `const char*` and builds `std::string`, or add a `static_assert` / unit test pinning the alternative chosen for a string literal.

### [INFO] INFO-6 Requirements Story 02.0/03.0 serializers must meet (string and byte values are firmware-controlled)
- **Strings:**
  - Bytes are arbitrary in 0x01-0xFF, with no embedded NUL by construction (split on NUL). A single string can be as long as the remaining table, up to the full table size, not 64 KiB.
  - Strings can be invalid UTF-8, overlong encodings, lone surrogates (CESU), C0/C1 controls, DEL, U+2028/U+2029, bidi overrides (U+202E, Trojan-Source-style display spoofing, CVE-2021-42574), and ANSI escape sequences.
  - Validate UTF-8 strictly and replace invalid sequences with U+FFFD before escaping (CWE-176).
  - **JSON:** escape U+0000-U+001F, `"` and `\`. Consider escaping U+2028/U+2029 for JS-embedding consumers.
  - **XML 1.0:** controls other than TAB/LF/CR are illegal even as character references, so they must be replaced, not just escaped. Escape `& < > " '`. Never emit a DOCTYPE. Firmware text must never become an element or attribute *name*; keys are constants today, so keep it that way (CWE-91).
  - **YAML:** always emit double-quoted scalars. Otherwise values such as `yes`, `null`, `~`, `0x1F`, `- `, `: `, `&anchor`, `*alias`, `!!tag` or a leading `%`/`@`/`` ` `` change type or structure (CWE-116).
  - **CSV:** quote fields per RFC 4180 and double embedded quotes. Neutralise formula injection: prefix values beginning with `=`, `+`, `-`, `@`, TAB or CR with `'` (CWE-1236). `-` is common in real values, so document the choice.
  - **Text/terminal output:** strip or escape ESC/C1 controls to prevent terminal-escape injection (CWE-150). This also applies to the existing `render_to_description()` path.
- **Composite strings:** `firmware_inventory_information_entry.cpp:268-269` concatenates firmware text with a decoded format string. A serializer cannot tell them apart, so don't parse it back.
- **Raw bytes:** each `std::vector<uint8_t>` field is at most 255 bytes today. Encode as hex or base64, never as a raw string. Serializers should still enforce a generic cap so a future source (e.g. Type 42/43 payloads) can't produce oversized output.
- **Integers:** `uint64_t` values above 2^53 (e.g. 64-bit memory-error addresses and extended sizes) lose precision in JSON consumed by JavaScript or doubles. Consider string-encoding them, or document the limit.
- **Duplicate keys:** names are unique within an entry by contract (C1), and the coverage test enforces it. Serializers should still assert uniqueness rather than silently overwrite.
- **Sentinels:** "Bad index", "Not Specified" and "Unknown" are in-band strings that look the same as a firmware string with that text. If consumers need to tell them apart, add an out-of-band marker.
- **Error disclosure:** don't echo raw firmware strings into exception messages or logs without the same sanitisation.

## Spoofing / Repudiation / Elevation

Not applicable. This is a read-only, in-process decoder with no authentication, no
privileged actions, no network and no persisted state. Privileged reads (`/dev/mem`)
are unchanged by this diff.

## Verdict: PASS_WITH_FOLLOWUP (CONCERNS)

There are no CRITICAL findings and no new out-of-bounds reads, so merge is not blocked.

Fix HIGH-1 (a four-line loop-variable change plus a regression test) before Story 02.0's
serializers start calling `get_fields()` on every entry. The bug hangs and runs out of
memory on a spec-legal firmware value and on any crafted `--read-file` dump. Track
MEDIUM-2 (fuzz the decoders) as an entry criterion for Story 02.0. Fold INFO-6 into the
serializer acceptance criteria.

Handoffs: fixes to `cpp-expert` (HIGH-1, LOW-4, LOW-5); regression and fuzz coverage to
`testing-expert` (HIGH-1 test, MEDIUM-2 harness extension).

---

## Story 02.0 review - 2026-11-10

Scope: `git diff 04e93f4..HEAD -- libsmbios tests` (code commits `f3447b2`, `edc344b`, `92a768e`):
`formatter_escaping.cpp`, `xml_formatter.cpp`, `json_formatter.cpp`, `text_formatter.cpp`,
`output_formatter.cpp`, and the Task 03 test validators.

### Assets & trust boundaries (delta)
Firmware strings and byte payloads (untrusted, any byte 0x01-0xFF, length up to the table size)
→ `get_fields()` → `for_each_code_point` (strict UTF-8) → per-format escaper → document text
handed to the caller (CLI stdout, `smbios_c` consumers, files). Field names and type labels are
library constants. Format names are code constants. Nothing is logged or persisted.

### Requirement verification

| Prior item | Status | Evidence |
|---|---|---|
| INFO-6: strict UTF-8 with U+FFFD | **Met** | `formatter_escaping.cpp:42-117`. The lead-byte classes match Unicode Table 3-7. Narrowing the second-byte range rejects E0/F0 overlongs (`:68-70`, `:78-80`), ED surrogates (`:71-73`) and anything above U+10FFFF on F4 (`:81-83`). C0/C1/F5-FF/stray continuations are rejected at `:85-91`. Every read is guarded (`:47`, `:96`) and each step advances at least 1 byte (`:115`). `append_utf8` is only ever given scalar values. Tests: `formatter_escaping_test.cpp:116-176` (maximal subparts, all 256 single bytes, property sweep). |
| INFO-6: XML forbidden code points replaced, never referenced | **Met** | `formatter_escaping.cpp:157-163,180-183`. The forbidden set (C0 except TAB/LF/CR, plus U+FFFE/U+FFFF) is exactly the complement of XML 1.0 `Char` once surrogates are excluded. No `&#` is emitted anywhere. The only fixed markup is the XML declaration (`xml_formatter.cpp:75`). No DOCTYPE or other PI is ever emitted, which `xml_formatter_test.cpp:177,256` and the validator's `<!`/`<?` rejection (`structured_output_validators.cpp:282-284`) assert. |
| INFO-6: no firmware text in names (CWE-91) | **Met** | Element names are literals. Field name and type are emitted only as escaped, double-quoted attribute values (`xml_formatter.cpp:51,59`), and `"` is escaped (`formatter_escaping.cpp:188`). All 45 `get_type()` implementations return string literals. `get_fields_coverage_test.cpp` `FieldNamesAreFirmwareIndependent` checks that names are printable ASCII and the same under hostile strings. |
| INFO-6: JSON escaping complete | **Met** | `formatter_escaping.cpp:200-218` escapes `"`, `\` and all of U+0000-U+001F (short forms or `\u00xx`), plus U+2028/U+2029, which satisfies RFC 8259 §7. Keys and the type label go through the same path (`json_formatter.cpp:56,64`). Integers are exact digits (`:38`), and the 2^53 limit is documented (`json_formatter.hpp` @warning). |
| INFO-6: YAML double-quoted escaper complete | **Met** (one INFO below) | `formatter_escaping.cpp:226-251`. Every code point outside YAML 1.2 `c-printable` is escaped: C0, DEL, C1 (with U+0085 as `\N`), U+FFFE and U+FFFF. U+2028/U+2029 are also escaped, as `\L`/`\P`. Every escape used is valid in both YAML 1.1 and 1.2. |
| INFO-6: raw bytes hex-encoded, generic cap | **Met by documented decision** | `hex_encode` (`formatter_escaping.cpp:255-264`) reserves exactly `2*size`. The multiplication cannot wrap, because `vector<uint8_t>::max_size() <= PTRDIFF_MAX`. There is no cap, and the reason is documented at `formatter_escaping.hpp:136-141`. I checked that every byte-vector source is bounded by one formatted area: `additional_information_entry.cpp:144`, `management_controller_host_interface_entry.cpp:102`, `system_boot_information_entry.cpp:75-82`, `processor_additional_information_entry.cpp:111`. Accepted. |
| LOW-3: clamp decision | **Met (decided and documented)** | Renderers never clamp (`status.md` "Carried forward"). Unclamped output is pinned by `xml_formatter_test.cpp:266`. Getter-side clamping is still an open follow-up, so LOW-3 itself stays open at entry level. |
| LOW-5: `const char*` → `bool` | **Met (test-pinned)** | `field_value_test.cpp:78-88` (`NamedFieldStringLiteralStaysStringTestCase`) fails on a pre-P1957 library. This is a runtime check rather than a compile-time one, which is acceptable for a robustness item. |
| Registry error messages leak nothing | **Met** | `output_formatter.cpp:28,38`. The messages contain only a null notice or the code-constant format name. No field value can reach them. |

### Findings

#### [LOW] SR-1 C1 controls (incl. U+009B CSI) are emitted raw in JSON and XML, contradicting the "use a structured format when output is not trusted" guidance
- Vector / evidence: `formatter_escaping.cpp:213-217` (JSON escapes only < U+0020) and `:157-163,190` (XML passes U+007F-U+009F through), as documented at `formatter_escaping.hpp:71-75,100-101`. `text_formatter.hpp:24-25` tells users to prefer `xml`/`json` for untrusted output. A firmware string containing `C2 9B 32 4A` (UTF-8 U+009B followed by `2J`) is valid UTF-8 and reaches stdout unchanged. Terminals that act on UTF-8-encoded C1 controls (e.g. xterm) interpret it as CSI.
- Impact: terminal escape injection when structured output is viewed directly in a terminal (CWE-150). The 7-bit ESC introducer is neutralised (ESC becomes `\u001b` in JSON and is replaced in XML); only the 8-bit C1 introducers are not. Document structure is unaffected.
- Mitigation (`cpp-expert`): in `json_escape`, also emit U+007F-U+009F as `\u00xx`. This is lossless and keeps the output valid JSON, with no contract change. For XML, keep the current behaviour, since C1 is legal data and character references are banned by policy. Reword `text_formatter.hpp:24-25` to say that structured formats guarantee *document structure*, not terminal display safety. Regression test (`testing-expert`): `json_escape("\xC2\x9B")` must equal `\u009b`.

#### [INFO] SR-2 YAML escaper passes U+FEFF (BOM) raw
- Vector / evidence: `formatter_escaping.cpp:242-250`. U+FEFF lies inside `c-printable`, so the output is spec-valid. YAML 1.2 §5.2, however, says a BOM must not appear inside a document, and some parsers strip it at line starts.
- Impact: interoperability and integrity only. Story 03.0 should not ship this behaviour unexamined.
- Mitigation: emit `\ufeff` in `yaml_double_quoted_escape` (lossless). Optionally do the same in JSON.

#### [INFO] SR-3 Registry is unsynchronised; `register_output_formatter` is public
- Vector / evidence: `output_formatter.cpp:17-41`, `output_formatter.hpp:59-70`. A plain `std::map` with no lock. Static initialisation within a module is single-threaded, and `get_output_formatter`/`registered_output_formatter_names` only read, so the intended use is race-free. A caller that registers after `main` while another thread looks up a formatter causes a data race (CWE-362). A duplicate registration throws during static init and calls `std::terminate`. That is intended fail-fast behaviour, but if it ever happened inside `smbios_c` it would abort the host process at `dlopen`. Format names are code constants, so this is reachable only through a programming error. `smbios_c`'s export allowlist keeps `register_output_formatter` out of the shared library's ABI.
- Mitigation: add "not thread-safe; static-init only" to the `register_output_formatter` doc comment. If Story 03.0 adds runtime or custom registration, add a mutex (or freeze the registry after init) at that point. The returned pointer is valid until static destruction, not "for the rest of the program's lifetime" (`output_formatter.hpp:75`); a doc nit.

#### [INFO] SR-4 XML end-of-line and attribute normalisation change CR/TAB/LF on round trip
- Vector / evidence: `formatter_escaping.cpp:157-163` passes raw TAB/LF/CR through, as documented at `formatter_escaping.hpp:75-78`. A conforming parser reads CR in character data as LF. Attribute values are constants (printable ASCII, test-enforced), so they are unaffected.
- Impact: low-grade integrity loss for firmware strings that contain CR. Not exploitable.
- Mitigation: accepted as documented. Revisit only if exact XML round-tripping becomes a requirement.

#### [INFO] SR-5 Test validators are safe to reuse
- `structured_output_validators.cpp`: JSON recursion is capped at depth 256 (`:72,92`). The XML validator is iterative, with an explicit stack (`:274`). Allocation is linear in input size: decoded strings take at most 4 bytes per input byte, plus the key set. Every `substr`/index use is bounded. One nit: after a trailing backslash, `string()` advances `pos_` past `size()` (`:176-178`) before returning `false`. It never reads there, but a fuzz reuse should keep the "return immediately" invariant. The decoder (`:27-52`) is independent of the library's decoder, so the two genuinely cross-check each other.

### Accepted residual risks (status.md "Notes & decisions")
- **No runtime duplicate-key assert: agree.** Names are library constants, not firmware-derived, which `FieldNamesAreFirmwareIndependent` covers across all 45 types. `is_well_formed_json` rejects duplicate keys after decoding escapes. Revisit if names ever come from data, as recorded.
- **`text` unsanitised under R3 (CWE-150 follow-up): agree, with conditions.** It is byte-identical to the pre-existing `render_to_description()` path, so it is not a regression. The header warning is in place. Conditions: (a) the follow-up must be tracked by an issue or roadmap item, not only a note; (b) the "prefer structured formats" advice must be corrected, or SR-1 fixed, because JSON and XML currently pass C1 controls too.

### Verdict: PASS_WITH_FOLLOWUP (CONCERNS)
No CRITICAL or HIGH findings, so merge is not blocked. INFO-6, LOW-3 (renderer-side decision) and LOW-5 are met. Follow-ups: SR-1 to `cpp-expert`, with a regression test to `testing-expert`; SR-2 before Story 03.0's YAML writer ships; SR-3 doc comment.

### Story 02.0 re-check - 2026-11-10
Scope: `git diff e7c2f75..c5bd074`.
- **SR-1 resolved.** `json_escape` now escapes U+007F..U+009F as `\u00XX` (`formatter_escaping.cpp:213-218`), with regression tests for U+009B. The XML header now says it guarantees well-formedness, not terminal safety, and recommends `json` (`formatter_escaping.hpp`, `xml_formatter.hpp`, `text_formatter.hpp`).
- **SR-2 resolved.** YAML maps U+FEFF to `﻿` (`formatter_escaping.cpp:245`), with a test.
- **SR-3 resolved.** The registry is documented as static-init only and not thread-safe, and the pointer is valid until static destruction (`output_formatter.hpp`). An empty format name is now rejected.
- **New code.** `render_document` only concatenates hooks that the formatter owns and skips null entries, so it adds no framing of its own. `field_value_to_string` returns strings unescaped, as documented. XML escapes every result, and JSON only adds quotes around `[0-9a-f]` hex. No diagnostic contains a field value. The only interpolated value is `format_name()`, which is a code constant. `SMBIOS_DETAIL_UNIQUE_NAME` (`__COUNTER__`) has no security impact.
- **Invisible Unicode scan.** No raw U+0080-U+009F, U+200B-U+200F, U+2028-U+202E, U+2066-U+2069, U+FEFF, U+FFFE or U+FFFF in `libsmbios/`, `tests/unit/` or this document.
- **Tests.** `smbios_unit_test` (build/macos): 602/602 cases pass.
- Verdict: **PASS**.
