# Security

Threat models, security review outputs, and posture documentation for libbios.

The `security-auditor` agent owns this directory. Every change touching auth,
secrets, external integrations, or untrusted-input ingestion triggers a security
review whose output is stored here.

## Naming convention

`threat-model-<scope>.md` for threat models, `review-<scope>-<YYYY-MM-DD>.md`
for point-in-time reviews.

## What a threat model must contain

1. **Scope** - which components and trust boundaries are in scope.
2. **Assets** - what secrets, PII, and data are handled.
3. **Threat actors** - attacker profiles considered.
4. **STRIDE analysis** - Spoofing, Tampering, Repudiation, Info Disclosure, DoS, Elevation.
5. **Mitigations** - existing controls and open gaps.
6. **Verdict** - CRITICAL (merge blocked) / HIGH / MEDIUM / LOW / INFO.

## Security rules (non-negotiable)

- Never log secrets; rely on this project's log-redaction mechanism (if any)
  and verify it covers new sinks.
- Never hard-code credentials. Read from settings/env.
- Treat all untrusted external input as sensitive - no unredacted raw input
  in logs, exceptions, stored reports, or API error bodies.
- Untrusted input must never reach a shell, SQL string, `eval`, or an AI
  prompt without sanitisation/parameterisation.

> **SME REVIEW NEEDED (AI-drafted - verify before relying on this):**
>
> ## Initial threat model - libbios SMBIOS acquisition and parsing
>
> **Scope.**
>
> - The `smbios` static library: `smbios/src/smbios.cpp`, `win_bios.cpp`, `unix_bios.cpp`, `physical_memory.cpp`, `posix_physical_memory.cpp`, `win_physical_memory.cpp`, `abstract_smbios_entry.cpp` and the `*_entry.cpp` decoders.
> - The `smbios_util` CLI (`smbios_util/src/main.cpp`, `command_line_parser.cpp`).
> - Out of scope: there is no network, IPC, authentication or credential handling anywhere in the code base.
>
> **Trust boundaries.**
>
> 1. Firmware/OS → process. The SMBIOS table arrives as raw bytes, either from `GetSystemFirmwareTable('RSMB')` or from a physical-memory mapping (`/dev/mem` via Boost.Iostreams `mapped_file_source`, or `\device\physicalmemory` via `NtOpenSection`/`NtMapViewOfSection`). The process typically runs as root/admin to reach these sources.
> 2. Command line → file system. `--dump-file <path>` writes the raw table to an arbitrary path with that privilege. `--read-file <path>` is parsed but not yet implemented; once implemented, it makes any user-supplied file a table source.
>
> **Assets.**
>
> - The integrity of the reported hardware inventory.
> - The privileged process itself.
> - Read access to physical memory, which the process obtains and which must stay confined to the SMBIOS ranges.
> - The table contents: serial numbers, asset tags and UUID-class identifiers that act as device identifiers. Treat the table as mildly sensitive when dumping or logging it.
>
> **Threat actors.**
>
> - Malicious or buggy firmware, or a hypervisor that presents a crafted virtual SMBIOS table to a guest.
> - A local user who influences the CLI arguments of a privileged invocation (wrapper scripts, sudo rules).
> - Once `--read-file` exists: anyone who supplies a crafted dump file.
>
> **STRIDE findings (draft).**
>
> - **Tampering / memory safety (HIGH).** Firmware-controlled lengths and offsets are trusted without bounds checks:
>   - `count_smbios_structures()` dereferences a `uint16_t` before testing `offset < end_table`.
>   - `read_smbios_table()` copies a full `DMIHeader`, which is 4 wire bytes plus a pointer, from the current position.
>   - `parse_dmi_strings()` walks strings with no table-end bound.
>   - Typed entries `reinterpret_cast` `header.data` to per-revision structs gated only by `header.length`.
>   - `RawSMBIOSData::length` (Windows) is used as the table size without being compared to the returned buffer size.
>   - Crafted values yield out-of-bounds reads (CWE-125) in a privileged process.
> - **Tampering / integrity (MEDIUM).** `checksum32()` computes the intermediate checksum as `smbios_crc(0x10, 0x0F)`. That loop runs from 16 to 15 and never iterates, so the intermediate (`_DMI_`) checksum is never actually verified.
> - **Tampering / integrity (MEDIUM).** The `SMBIOSEntryPoint32` and `SMBIOSEntryPoint64` layouts in `smbios.h` diverge from the spec: `intptr_t` addresses change width with the build target, and the 64-bit struct lacks the entry-point-revision byte. Consequently `structure_table_address`/length can be read from the wrong bytes, and `/dev/mem` may be mapped at an unintended physical address.
> - **Information disclosure (LOW-MEDIUM).** In `posix_physical_memory.cpp`, `get_memory_dump()` returns bytes from the page-aligned start of the mapping rather than from the requested base. This exposes up to a page of unrelated physical memory to the parser and, via `--dump-file`, to disk. `--dump-file` writes device identifiers to a caller-chosen path with no permission control.
> - **Elevation (MEDIUM).** `--dump-file` performs a privileged write to any path, so it can clobber files when the binary is run via sudo or as a setuid wrapper. Review before any privileged packaging.
> - **DoS (LOW).** A missing `\0\0` terminator or `length < 4` ends parsing early. Unbounded walks past `end_table` can crash the tool. `SMBios::render_to_description()` dereferences `smbios_entry32_` on the 64-bit path (null-pointer dereference).
> - **Spoofing / Repudiation.** Not applicable: there is no authentication, no multi-user state, and no audit-relevant actions.
>
> **Existing controls.**
>
> - Read-only mappings (`mapmode::readonly`, `SECTION_MAP_READ`, `PAGE_READONLY`).
> - Entry-point checksums (partial, see above).
> - `header.length < 4` and `EndOfTable` termination.
> - Version- and length-gated field access in the entry decoders.
> - `dmi_string()` returns `"Bad index"` for out-of-range indices.
>
> **Open gaps to confirm.**
>
> - Bound every table walk by `get_table_size()`.
> - Fix the intermediate-checksum range.
> - Correct the packed entry-point layouts.
> - Honour the page offset in `get_memory_dump()`.
> - Validate the `RawSMBIOSData::length` value.
> - Define the trust and permission model for `--dump-file`/`--read-file` before implementing `--read-file`.
>
> **Verdict (draft):** HIGH. There is no CRITICAL finding because the attacker must already control firmware or the hypervisor, or (later) the dump file. However, memory-safety fixes should precede any feature that parses user-supplied dumps.
