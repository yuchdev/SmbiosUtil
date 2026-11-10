#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// Electrical Current Probe entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Electrical Current
// Probe' chapter (7.30), DSP0134 3.9.0. Field offsets (Table 101) and the
// Location and Status bit layout (7.30.1, Table 102) were confirmed
// directly against docs/standard/DSP0134_3.9.0.md, not assumed from the
// task document's own field list, and not assumed to be byte-identical to
// TemperatureProbeEntry's Table 100 despite the near-identical shape - see
// the cross-check note below.
//
// Status/Location bit-layout cross-check (per this task's own explicit
// instruction to confirm 7.27.1/7.29.1/7.30.1 independently): the Status
// sub-field (bits 7:5) is confirmed BYTE-IDENTICAL to VoltageProbeEntry's
// and TemperatureProbeEntry's own Status (same six values, same bit
// positions - Table 96/Table 100/Table 102 all agree). The Location
// sub-field (bits 4:0) is confirmed BYTE-IDENTICAL to VoltageProbeEntry's
// own 11-value Location list (Table 96 and Table 102 enumerate the
// identical 11 values at the identical bit positions) - NOT identical to
// TemperatureProbeEntry's own Location, which defines a 15-value superset
// (Table 100 adds four more values not present in Table 102). This class's
// own Location enum matches the Table 96/102 11-value list.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Electrical Current Probe structure (7.30) - a single current
/// probe's physical location/status, its description string, its readable
/// maximum/minimum/nominal values, resolution, tolerance and accuracy (all
/// in milliamps or sub-milliamp units per field, each independently
/// `0x8000`-sentineled "unknown"), and an OEM-defined DWORD.
class ElectricalCurrentProbeEntry : public AbstractSMBiosEntry {
public:

    /// @brief Status sub-field (bits 7:5 of Location and Status), DSP0134
    /// 3.9.0 Table 102 - confirmed byte-identical to VoltageProbeEntry's/
    /// TemperatureProbeEntry's own Status, see class doc comment.
    /// `StatusOutOfSpec` (value 0, binary 000) is not a spec value; this
    /// project's own fallback sentinel for "field absent or reserved 3-bit
    /// value" (binary 111 is likewise reserved and decodes to this same
    /// sentinel).
    enum Status : uint8_t {
        StatusOutOfSpec = 0x00,
        StatusOther = 0x01,
        StatusUnknown = 0x02,
        StatusOk = 0x03,
        StatusNonCritical = 0x04,
        StatusCritical = 0x05,
        StatusNonRecoverable = 0x06
    };

    /// @brief Location sub-field (bits 4:0 of Location and Status), DSP0134
    /// 3.9.0 Table 102 - confirmed byte-identical to VoltageProbeEntry's
    /// own 11-value Location, see class doc comment. `LocationOutOfSpec`
    /// (value 0) is not a spec value; this project's own fallback sentinel
    /// for "field absent or an unassigned 5-bit value".
    enum Location : uint8_t {
        LocationOutOfSpec = 0x00,
        LocationOther = 0x01,
        LocationUnknown = 0x02,
        LocationProcessor = 0x03,
        LocationDisk = 0x04,
        LocationPeripheralBay = 0x05,
        LocationSystemManagementModule = 0x06,
        LocationMotherboard = 0x07,
        LocationMemoryModule = 0x08,
        LocationProcessorModule = 0x09,
        LocationPowerUnit = 0x0A,
        LocationAddInCard = 0x0B
    };

    /// @brief Maximum/Minimum/Resolution/Tolerance/Accuracy/Nominal Value
    /// sentinel (0x8000): the value is unknown, per spec, for every one of
    /// these WORD fields.
    static constexpr uint16_t ValueUnknown = 0x8000;

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    ElectricalCurrentProbeEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~ElectricalCurrentProbeEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Same fields as render_to_description(), in the same order. Every
    /// WORD reading stays the printed string - "<value><unit>" or the
    /// "Unknown" sentinel (0x8000) - and Nominal Value appears exactly when
    /// the text prints it (has_nominal_value()). OEM-defined is not printed,
    /// so it is not a field.
    std::vector<NamedField> get_fields() const override;

    //////////////////////////////////////////////////////////////////////////
    // Byte/word/dword values

    /// @brief 0x05 offset, raw byte. See Status/Location enums and the
    /// class doc comment for the confirmed Table 102 bit layout. Returns 0
    /// if get_entry_size() < 0x06.
    uint8_t get_location_and_status() const;

    /// @brief 0x06 offset, 2 raw bytes, milliamps. ValueUnknown (0x8000)
    /// means unknown, per spec. Returns ValueUnknown if get_entry_size() <
    /// 0x08.
    uint16_t get_maximum_value() const;

    /// @brief 0x08 offset, 2 raw bytes, milliamps. ValueUnknown (0x8000)
    /// means unknown, per spec. Returns ValueUnknown if get_entry_size() <
    /// 0x0A.
    uint16_t get_minimum_value() const;

    /// @brief 0x0A offset, 2 raw bytes, tenths of milliamps. ValueUnknown
    /// (0x8000) means unknown, per spec. Returns ValueUnknown if
    /// get_entry_size() < 0x0C.
    uint16_t get_resolution() const;

    /// @brief 0x0C offset, 2 raw bytes, +/- milliamps. ValueUnknown
    /// (0x8000) means unknown, per spec. Returns ValueUnknown if
    /// get_entry_size() < 0x0E.
    uint16_t get_tolerance() const;

    /// @brief 0x0E offset, 2 raw bytes, +/- 1/100th of a percent.
    /// ValueUnknown (0x8000) means unknown, per spec. Returns ValueUnknown
    /// if get_entry_size() < 0x10.
    uint16_t get_accuracy() const;

    /// @brief 0x10 offset, 4 raw bytes. OEM- or firmware vendor-specific
    /// information. Returns 0 if get_entry_size() < 0x14.
    uint32_t get_oem_defined() const;

    /// @brief 0x14 offset, 2 raw bytes, milliamps. ValueUnknown (0x8000)
    /// means unknown, per spec. Present only if has_nominal_value() is
    /// true; returns ValueUnknown otherwise.
    uint16_t get_nominal_value() const;

    //////////////////////////////////////////////////////////////////////////
    // String values

    /// @brief Description DMI string (0x04); additional descriptive
    /// information about the probe or its location.
    std::string get_description_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Decoded Location and Status sub-fields (Table 102)

    /// @brief Bits 7:5 of Location and Status. See Status for values.
    /// Returns StatusOutOfSpec if get_entry_size() < 0x06.
    Status get_status() const;

    /// @brief String representation of get_status().
    std::string get_status_string() const;

    /// @brief Bits 4:0 of Location and Status. See Location for values.
    /// Returns LocationOutOfSpec if get_entry_size() < 0x06.
    Location get_location() const;

    /// @brief String representation of get_location().
    std::string get_location_string() const;

    //////////////////////////////////////////////////////////////////////////
    // Predicates

    /// @brief Whether Maximum Value (0x06) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x08.
    bool is_maximum_value_known() const;

    /// @brief Whether Minimum Value (0x08) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x0A.
    bool is_minimum_value_known() const;

    /// @brief Whether Resolution (0x0A) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x0C.
    bool is_resolution_known() const;

    /// @brief Whether Tolerance (0x0C) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x0E.
    bool is_tolerance_known() const;

    /// @brief Whether Accuracy (0x0E) is known, i.e. not ValueUnknown.
    /// Returns false if get_entry_size() < 0x10.
    bool is_accuracy_known() const;

    /// @brief Whether Nominal Value (0x14) is physically present in this
    /// structure. Per spec text ("present ... only if the structure's
    /// length is larger than 14h") the gate is length > 0x14, but since
    /// Nominal Value is a 2-byte WORD, a length of exactly 0x15 could only
    /// supply its low byte - this accessor therefore requires the full
    /// field (get_entry_size() >= 0x16) before reporting it present.
    bool has_nominal_value() const;

    /// @brief Whether Nominal Value (0x14) is known, i.e. physically
    /// present (has_nominal_value()) and not ValueUnknown.
    bool is_nominal_value_known() const;

private:

    /// Map Status/Location values to string values
    void init_string_values();

private:

    /// Bitwise/code to string representation
    std::map<uint8_t, std::string> status_map_;
    std::map<uint8_t, std::string> location_map_;
};

} // namespace smbios
