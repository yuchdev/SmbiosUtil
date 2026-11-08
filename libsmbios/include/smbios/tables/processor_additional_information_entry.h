#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <smbios/abstract_smbios_entry.h>

// Processor Additional Information entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Processor Additional
// Information' chapter (7.45), DSP0134 3.9.0. Field offsets (Table 128),
// the Processor-specific Block format (7.45.1, Table 129), and the
// Processor Architecture Types enum (Table 130) were confirmed directly
// against docs/standard/DSP0134_3.9.0.md.
//
// DOUBLE-LENGTH RELATIONSHIP - READ CAREFULLY BEFORE TOUCHING THIS CLASS:
// this one structure carries TWO distinct, differently-scoped length
// fields, and they are easy to conflate:
//   - The OUTER structure's own standard DMI header Length byte (0x01 of
//     the structure as a whole) is "6 + Y" per Table 128, where Y is the
//     length of the Processor-specific Block starting at offset 0x06. This
//     is get_entry_size() (inherited from AbstractSMBiosEntry) - never
//     touched directly by this class's own accessors, only used as a
//     bounds guard.
//   - The Processor-specific Block's OWN Block Length byte (+0x00 of that
//     block, i.e. structure offset 0x06), called "N" by Table 129, is the
//     length of the Processor-Specific Data that follows it (+0x02 of the
//     block, i.e. structure offset 0x08) - NOT the length of the whole
//     block. The block itself is therefore N + 2 bytes wide (Block Length
//     byte + Processor Type byte + N data bytes), so Y = N + 2.
//   - Substituting Y = N + 2 into the outer Length formula gives the exact
//     arithmetic relationship this class's own unit tests assert
//     explicitly: outer Length = 6 + Y = 6 + (N + 2) = 8 + N.
// get_block_length() below returns N (the sub-header's own byte, read at
// structure offset 0x06) - it is NOT derived from, and must never be
// confused with, the outer structure's Length field.
//
// SCOPE CUTOFF (R8, see this story's own README "Key constraints", same
// reasoning as System Event Log's (Story 03.0) and Additional
// Information's (Story 06.0) own documented cutoffs):
// get_processor_specific_data() returns the Processor-Specific Data field
// (structure offset 0x08, N raw bytes) as opaque bytes only. 7.45.2's own
// text is explicit that the per-architecture format of this payload (x86
// CPUID-derived fields, ARM implementer-specific fields, RISC-V vendor
// extensions, LoongArch vendor extensions, etc.) is "maintained by
// processor architecture workgroups or vendors in separate documents" -
// entirely outside DSP0134 3.9.0's own scope, let alone this project's.
// This class therefore does not attempt to decode that payload for any
// Processor Type.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Processor Additional Information structure (7.45) - one
/// instance per processor-specific block of additional data describing a
/// related SMBIOS Type 4 (Processor Information) structure, referenced via
/// Referenced Handle (0x04). There may be multiple Type 44 structures
/// referencing the same Type 4 structure (e.g. one per non-identical core).
///
/// See this header's own leading comment block for the double-length
/// relationship between the outer structure's Length field and the
/// Processor-specific Block's own Block Length byte (N), and for this
/// class's documented scope cutoff (R8) on Processor-Specific Data.
class ProcessorAdditionalInformationEntry : public AbstractSMBiosEntry {
public:

    /// @brief Processor Architecture Types (7.45.1, Table 130). Confirmed
    /// against the full table in docs/standard/DSP0134_3.9.0.md - not left
    /// at the placeholder 0x00 = Reserved entry alone. Any byte value not
    /// listed here (e.g. 0x0B and above) is unassigned as of DSP0134 3.9.0
    /// and is reported by get_processor_type_string() as "Unknown".
    enum ProcessorType : uint8_t {
        Reserved = 0x00,
        Ia32 = 0x01,
        X64 = 0x02,
        ItaniumArchitecture = 0x03,
        Arm32 = 0x04,
        Arm64 = 0x05,
        RiscV32 = 0x06,
        RiscV64 = 0x07,
        RiscV128 = 0x08,
        LoongArch32 = 0x09,
        LoongArch64 = 0x0A
    };

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    ProcessorAdditionalInformationEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~ProcessorAdditionalInformationEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Raw values

    /// @brief 0x04 offset, raw WORD. Handle, or instance number, of the
    /// related SMBIOS Type 4 (Processor Information) structure. Exposed as
    /// a raw handle only - this class does not resolve it to the referenced
    /// structure itself (no cross-structure lookup mechanism exists at this
    /// level). Returns 0 if get_entry_size() < 0x06.
    uint16_t get_referenced_handle() const;

    /// @brief 0x06 offset (+0x00 of the Processor-specific Block), raw
    /// BYTE: Block Length, i.e. "N" - the length of the Processor-Specific
    /// Data field that follows (+0x02 of the block, structure offset
    /// 0x08). See this header's own leading comment block for the exact
    /// arithmetic relationship to the outer structure's own Length field
    /// (outer Length == 8 + N) - this accessor returns N itself, read
    /// directly from the sub-header's own byte, never derived from the
    /// outer Length. Returns 0 if get_entry_size() < 0x07.
    uint8_t get_block_length() const;

    /// @brief 0x07 offset (+0x01 of the Processor-specific Block), raw
    /// BYTE enum (Table 130). The processor architecture delineated by
    /// this Processor-specific Block. Returns ProcessorType::Reserved (0)
    /// if get_entry_size() < 0x08.
    uint8_t get_processor_type() const;

    /// @brief String representation of get_processor_type(), per the
    /// confirmed full Table 130. Returns "Unknown" for any byte value not
    /// listed in the ProcessorType enum (unassigned as of DSP0134 3.9.0).
    std::string get_processor_type_string() const;

    /// @brief 0x08 offset (+0x02 of the Processor-specific Block), N raw
    /// BYTEs where N == get_block_length(). Processor-Specific Data -
    /// returned as opaque raw bytes only; see this header's own leading
    /// comment block for the documented scope cutoff (R8). Returns an
    /// empty vector if the full N bytes are not available within
    /// get_entry_size() (e.g. a truncated/malformed structure).
    std::vector<uint8_t> get_processor_specific_data() const;

private:

    /// Map ProcessorType enum values to string values
    void init_string_values();

private:

    /// Processor Type to string representation
    std::map<uint8_t, std::string> processor_type_map_;
};

} // namespace smbios
