# 02 - Processor Additional Information

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Implement `ProcessorAdditionalInformationEntry` (`SMBiosHandler` value
`44`), DSP0134 3.9.0 section 7.45, decoding the Processor-Specific
Block's own sub-header (7.45.1) and leaving the vendor-defined payload
inside it opaque (see this story's own documented scope cutoff).

## Files

- **Create**
  `libsmbios/include/smbios/tables/processor_additional_information_entry.h` /
  `.cpp`:
  - 0x04 Referenced Handle (WORD, references a Type 4 Processor
    Information structure)
  - 0x06 Processor-Specific Block (variable length `Y`, sub-shaped per
    7.45.1):
    - +0x00 Block Length (BYTE, value `N`)
    - +0x01 Processor Type (BYTE, ENUM - see Table 130; confirm the full
      value list against the spec directly - this task's own excerpt
      showed only the `0x00 = Reserved` entry, with further rows implied
      by "see 7.45.2" but not shown)
    - +0x02 Processor-Specific Data (`N` raw BYTEs, opaque per scope
      cutoff)
- **Modify** `smbios/src/command_line_parser.cpp` / `main.cpp`: display
  support.

## Functions and Classes

- `ProcessorAdditionalInformationEntry` - new. `get_referenced_handle()`,
  `get_block_length()` (the sub-header's own Block Length byte, `N` -
  distinct from the outer structure's `Length` field, which is `6 + Y`
  where `Y = 2 + N`; document this double-length relationship clearly in
  the class doc comment since it is easy to conflate the two), 
  `get_processor_type()` + `get_processor_type_string()`,
  `get_processor_specific_data()` (returns `std::vector<uint8_t>` of
  length `N`, raw, per scope cutoff).

## Constraints

- **Explicit scope cutoff (R8)**: `get_processor_specific_data()`
  returns opaque raw bytes. This class does not decode any
  architecture-specific payload (x86 CPUID-derived fields, ARM
  implementer-specific fields, RISC-V vendor extensions, etc.) - those
  formats are defined in documents outside DSP0134 3.9.0 itself (the
  spec's own 7.45.2 text says so explicitly), and this project has no
  per-architecture decode table for any of them. Document this in the
  class's own doc comment, same pattern as System Event Log's (Story
  03.0) and Additional Information's (Story 06.0) cutoffs.
- Confirm Table 130's full Processor Architecture Types enum list against
  the complete spec table before finalizing `get_processor_type_string()`
  - this task's own excerpt only showed the `0x00 = Reserved` row; later
  rows (likely at least x86, x64, and other common architecture values)
  exist in the full table and must not be guessed.
- `get_block_length()` (`N`, the sub-header field) must not be confused
  with the outer structure's own `Length` byte (`6 + Y` where
  `Y = 2 + N`) - both exist in this one structure and have a precise
  arithmetic relationship worth asserting in at least one test (Length
  of the outer structure equals `8 + N`).

## Tests

- `tests/unit/processor_additional_information_entry_test.cpp` (new): a
  buffer with a small (`N == 1` or `2`) opaque data block, verifying the
  outer `Length`/inner `Block Length` arithmetic relationship holds, and
  at least one documented Processor Type enum value.

## Success criteria

- [ ] Processor Architecture Types enum confirmed against the full spec
      table (not left at the placeholder `0x00 = Reserved` entry alone).
- [ ] Outer `Length`/inner `Block Length` relationship verified by an
      explicit test assertion.
- [ ] Registers and resolves through the factory for `SMBiosHandler ==
      44`.
- [ ] `smbios` CLI displays the type on real hardware where present.
