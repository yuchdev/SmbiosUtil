#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <libsmbios/abstract_smbios_entry.h>
#include <libsmbios/smbios.h>

// System Event Log entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'System Event Log' chapter
// (7.16), DSP0134 3.9.0. Field offsets through 0x10 (Access Method Address)
// and the 2.1+ fields at 0x14-0x16 plus the trailing descriptor list were
// confirmed directly against docs/standard/DSP0134_3.9.0.md's own section
// 7.16 (Table 60).
//
// The task document that drove this class flagged its own gap: its
// reproduction of Table 60 omitted the 0x0A "Access Method" row, redacted by
// the markdown-extraction tool that produced DSP0134_3.9.0.md from the
// original PDF. Reading Table 60 directly (docs/standard/DSP0134_3.9.0.md,
// grep "7.16") confirms Access Method sits exactly where the surrounding
// fields' own offset arithmetic implies - a BYTE at offset 0x0A, between Log
// Data Start Offset (0x08, a WORD, ending at 0x0A) and Log Status (0x0B) -
// with these defined values: 00h "Indexed I/O: 1 8-bit index port, 1 8-bit
// data port", 01h "Indexed I/O: 2 8-bit index ports, 1 8-bit data port", 02h
// "Indexed I/O: 1 16-bit index port, 1 8-bit data port", 03h "Memory-mapped
// physical 32-bit address", 04h "Available through General-Purpose
// NonVolatile Data functions", 05h-7Fh reserved for future assignment by the
// specification, 80h-FFh firmware vendor/OEM-specific. No discrepancy was
// found once the real table row was read - the redaction was purely a
// markdown-extraction artifact, not evidence of a different real offset.
//
// Explicit scope cutoff (R8): this class decodes the System Event Log
// *structure* only - its header fields, status bits, and (2.1+) list of
// supported event-log-type descriptors. It does NOT read, access, or decode
// the actual event-log records living in the non-volatile storage area that
// Access Method Address merely points to (7.16.2 Indexed I/O access,
// 7.16.3 Access Method Address layout, 7.16.4 Event Log organization,
// 7.16.5 Log Header format, 7.16.6 Log Record format). Doing so would
// require indexed-I/O, memory-mapped, or GPNV-handle access this codebase
// has no existing OS abstraction for - distinct from, and lower-level than,
// the ISmbiosSource chain (milestone 0005), which reads the SMBIOS
// *structure table* itself, not arbitrary NVRAM the table merely
// references. get_access_method_address() exposes the raw pointer/index
// value for callers that have their own out-of-band access mechanism; this
// class never dereferences it.

namespace smbios {

struct DMIHeader;

/// @brief The System Event Log structure (7.16) - a fixed header (Log Area
/// Length, Log Header/Data Start Offsets, Access Method, Log Status, Log
/// Change Token, Access Method Address) plus, for SMBIOS 2.1 and later, a
/// Log Header Format byte, a count (`x`) and per-entry length (`y`) for a
/// trailing list of Supported Event Log Type Descriptors occupying
/// `0x17..0x17+(x*y)-1`.
///
/// Scope cutoff: see this header's own leading comment block (R8) - only
/// the structure's own fields are decoded here, never the event-log
/// records the structure points to via Access Method Address.
///
/// Version-gating discipline: every 2.1+ field (0x14 onward) is guarded by
/// BOTH `SMBiosVersion >= {2, 1}` AND `get_entry_size()` - a version-2.0
/// structure reports `Length == 0x14` and genuinely carries none of these
/// fields, but a too-short buffer paired with an inflated version number
/// must still never be read past 0x14 either (same double-guard discipline
/// FirmwareLanguageInformationEntry::get_flags() established for its own
/// 2.1+ Flags byte).
///
/// Bounds-safety note on the trailing descriptor list: per 7.16's own
/// Table 60 note, "If the value [at 0x15] is 0, the list that starts at
/// offset 17h is not present" - `x == 0` is a legitimate, common case, not
/// an error, and get_supported_descriptor_count() reports 0 for it rather
/// than treating it as "unavailable". get_log_type_descriptor_at() is, like
/// every variable-length-trailing-array accessor elsewhere in this project,
/// independently guarded against get_entry_size() directly (not merely
/// against get_supported_descriptor_count()), so a malformed `x`/`y` pair
/// can never cause an out-of-bounds read.
class SystemEventLogEntry : public AbstractSMBiosEntry {
public:

    // @brief Access Method field (offset 0x0A), see Table 60 and this
    // header's own leading comment for how the redacted row was confirmed.
    enum AccessMethod : uint8_t {
        OneIndexOneDataPort8Bit = 0x00,
        TwoIndexOneDataPort8Bit = 0x01,
        OneIndexOneDataPort16Bit = 0x02,
        MemoryMappedPhysical32Bit = 0x03,
        GeneralPurposeNonVolatileData = 0x04
    };

    // @brief Log Status bitmask (offset 0x0B), see Table 60: bit 0 = log
    // area valid, bit 1 = log area full, bits 7:2 reserved.
    enum LogStatus : uint8_t {
        LogAreaValid = 0x01,
        LogAreaFull = 0x02
    };

    /// @brief Parse the header; recognizes how much information is present
    /// depending on both get_entry_size() and the table's own SMBiosVersion
    /// (R8/version-gating note above).
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    SystemEventLogEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~SystemEventLogEntry() = default;

    /// @brief String representation
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word/dword values

    /// @brief 0x04 offset, WORD
    /// Length, in bytes, of the overall event-log area, from the first byte
    /// of header to the last byte of data; 0 if get_entry_size() < 0x06
    uint16_t get_log_area_length() const;

    /// @brief 0x06 offset, WORD
    /// Starting offset (or index), from Access Method Address, of the
    /// event log's header; 0 if get_entry_size() < 0x08
    uint16_t get_log_header_start_offset() const;

    /// @brief 0x08 offset, WORD
    /// Starting offset (or index), from Access Method Address, of the
    /// event log's first data byte; 0 if get_entry_size() < 0x0A
    uint16_t get_log_data_start_offset() const;

    /// @brief 0x0A offset, BYTE
    /// See AccessMethod enum for the defined values below 0x80; 0x80-0xFF
    /// are firmware vendor/OEM-specific. 0 if get_entry_size() < 0x0B.
    uint8_t get_access_method() const;

    /// @brief 0x0B offset, BYTE
    /// Raw Log Status bitmask; see LogStatus enum and
    /// is_log_area_valid()/is_log_area_full(). 0 if get_entry_size() < 0x0C.
    uint8_t get_log_status() const;

    /// @brief Bit 0 of get_log_status(): true if the log area is valid.
    bool is_log_area_valid() const;

    /// @brief Bit 1 of get_log_status(): true if the log area is full.
    bool is_log_area_full() const;

    /// @brief 0x0C offset, DWORD
    /// Unique token reassigned every time the event log changes; 0 both
    /// when not implemented by the firmware and when get_entry_size() <
    /// 0x10 (the two cases are indistinguishable from this field alone).
    uint32_t get_log_change_token() const;

    /// @brief 0x10 offset, DWORD
    /// Raw address/index associated with get_access_method() - format
    /// depends on that field's value (indexed-I/O port pair, 32-bit
    /// physical address, or GPNV handle). Never dereferenced by this class;
    /// see the scope-cutoff note in this header's leading comment (R8).
    /// 0 if get_entry_size() < 0x14.
    uint32_t get_access_method_address() const;

    /// @brief 0x14 offset, BYTE, ENUM (2.1+)
    /// Format of the log header area (see 7.16.5); 0 if the table's
    /// SMBiosVersion predates 2.1 or get_entry_size() < 0x15.
    uint8_t get_log_header_format() const;

    /// @brief 0x15 offset, BYTE (2.1+)
    /// Number of Supported Log Type Descriptors ("x") that follow at 0x17;
    /// 0 if the table's SMBiosVersion predates 2.1, get_entry_size() <
    /// 0x16, or the firmware genuinely reports no descriptors (a legitimate
    /// case per Table 60's own note - see class doc comment).
    uint8_t get_supported_descriptor_count() const;

    /// @brief 0x16 offset, BYTE (2.1+)
    /// Length, in bytes, of each entry in the trailing descriptor list
    /// ("y"); the spec documents this as "currently always 2", but this
    /// accessor returns the actual byte read, never a hardcoded 2 - see
    /// get_log_type_descriptor_at()'s own comment for why the offset
    /// arithmetic below it must use this value, not a literal. 0 if the
    /// table's SMBiosVersion predates 2.1 or get_entry_size() < 0x17.
    uint8_t get_descriptor_length() const;

    /// @brief One raw descriptor entry from the trailing list at
    /// `0x17 + index*get_descriptor_length()`, `get_descriptor_length()`
    /// bytes long (7.16.1: byte 0 Log Type, byte 1 Variable Data Format
    /// Type, when get_descriptor_length() == 2 as currently specified).
    /// Independently guarded against get_entry_size() (not merely against
    /// get_supported_descriptor_count()), so a malformed x/y pair can never
    /// cause an out-of-bounds read.
    /// @param index 0-based index into the trailing descriptor list.
    /// @return the raw get_descriptor_length()-byte descriptor at `index`,
    /// little-endian-packed into a uint32_t (byte 0 in the low byte); 0 if
    /// the table's SMBiosVersion predates 2.1, get_descriptor_length() is
    /// 0, or the read would fall at or beyond get_entry_size().
    uint32_t get_log_type_descriptor_at(size_t index) const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Human-readable name for get_access_method(); falls back to
    /// "Reserved" for 0x05-0x7F and "OEM-specific (N)" for 0x80-0xFF.
    std::string get_access_method_string() const;

private:

    /// Populate access_method_names_ with the defined AccessMethod values.
    void init_string_values();

private:

    /// Copy of the SMBIOS version, needed for the 2.1+ fields' version
    /// gating (every field below 0x14 is distinguished by header.length
    /// alone).
    SMBiosVersion version_;

    /// AccessMethod value -> human-readable name.
    std::map<uint8_t, std::string> access_method_names_;
};

} // namespace smbios
