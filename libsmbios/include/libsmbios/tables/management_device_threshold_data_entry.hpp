#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <libsmbios/abstract_smbios_entry.hpp>

// Management Device Threshold Data entry
// See http://www.dmtf.org/standards/smbios
// Standard according to current SMBIOS version, 'Management Device
// Threshold Data' chapter (7.37), DSP0134 3.9.0. Field offsets (Table 111)
// were confirmed directly against docs/standard/DSP0134_3.9.0.md.
namespace smbios {

struct DMIHeader;
struct SMBiosVersion;

/// @brief The Management Device Threshold Data structure (7.37) - six
/// threshold WORD fields (lower/upper non-critical, lower/upper critical,
/// lower/upper non-recoverable) for a component (probe or cooling-unit)
/// contained within a Management Device, each independently `0x8000`-
/// sentineled "unavailable" per spec text ("If the value is unavailable,
/// the field is set to 8000h.").
///
/// Deliberately carries NO unit information of its own: per 7.37's own
/// text, "the threshold units (millivolts, milliamps, 1/10th degrees C, or
/// RPMs) are as defined by the associated probe or cooling-unit component
/// structure" - i.e. whichever structure a ManagementDeviceComponentEntry's
/// own Threshold Handle points this structure's own handle at. This class
/// therefore exposes only raw numeric WORD values and intentionally does
/// not provide a `get_unit_string()` or similar; resolving which probe or
/// cooling-device structure (if any) references a given threshold-data
/// handle is outside this class's scope, matching this codebase's existing
/// "expose raw handles, do not resolve them" precedent.
class ManagementDeviceThresholdDataEntry : public AbstractSMBiosEntry {
public:

    /// @brief Shared "unavailable" sentinel (0x8000) for every one of this
    /// structure's six WORD fields, per 7.37's own spec text.
    static constexpr uint16_t ValueUnknown = 0x8000;

    /// @brief Parse the header.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table, forwarded to AbstractSMBiosEntry for bounds-checked
    /// string parsing.
    ManagementDeviceThresholdDataEntry(const DMIHeader& header, const SMBiosVersion& version, const uint8_t* table_end);

    // @brief Parent is abstract
    virtual ~ManagementDeviceThresholdDataEntry() = default;

    /// @brief String representation of the structure kind.
    virtual std::string get_type() const override;

    /// @brief Render all entry information into single string
    virtual std::string render_to_description() const override;

    /// @brief Structured view of this entry (milestone 0009 contract C1).
    /// Same six fields as render_to_description(), in the same order. Each
    /// stays the printed string (a decimal or the "Unknown" sentinel), since
    /// every threshold can print that sentinel.
    std::vector<NamedField> get_fields() const override;

    //////////////////////////////////////////////////////////////////////////
    // Word values

    /// @brief 0x04 offset, raw word. ValueUnknown (0x8000) means
    /// unavailable, per spec. Returns ValueUnknown if get_entry_size() <
    /// 0x06.
    uint16_t get_lower_threshold_non_critical() const;

    /// @brief 0x06 offset, raw word. ValueUnknown (0x8000) means
    /// unavailable, per spec. Returns ValueUnknown if get_entry_size() <
    /// 0x08.
    uint16_t get_upper_threshold_non_critical() const;

    /// @brief 0x08 offset, raw word. ValueUnknown (0x8000) means
    /// unavailable, per spec. Returns ValueUnknown if get_entry_size() <
    /// 0x0A.
    uint16_t get_lower_threshold_critical() const;

    /// @brief 0x0A offset, raw word. ValueUnknown (0x8000) means
    /// unavailable, per spec. Returns ValueUnknown if get_entry_size() <
    /// 0x0C.
    uint16_t get_upper_threshold_critical() const;

    /// @brief 0x0C offset, raw word. ValueUnknown (0x8000) means
    /// unavailable, per spec. Returns ValueUnknown if get_entry_size() <
    /// 0x0E.
    uint16_t get_lower_threshold_non_recoverable() const;

    /// @brief 0x0E offset, raw word. ValueUnknown (0x8000) means
    /// unavailable, per spec. Returns ValueUnknown if get_entry_size() <
    /// 0x10.
    uint16_t get_upper_threshold_non_recoverable() const;

    //////////////////////////////////////////////////////////////////////////
    // Predicates

    /// @brief Whether Lower Threshold - Non-critical (0x04) is known, i.e.
    /// not ValueUnknown. Returns false if get_entry_size() < 0x06.
    bool is_lower_threshold_non_critical_known() const;

    /// @brief Whether Upper Threshold - Non-critical (0x06) is known, i.e.
    /// not ValueUnknown. Returns false if get_entry_size() < 0x08.
    bool is_upper_threshold_non_critical_known() const;

    /// @brief Whether Lower Threshold - Critical (0x08) is known, i.e. not
    /// ValueUnknown. Returns false if get_entry_size() < 0x0A.
    bool is_lower_threshold_critical_known() const;

    /// @brief Whether Upper Threshold - Critical (0x0A) is known, i.e. not
    /// ValueUnknown. Returns false if get_entry_size() < 0x0C.
    bool is_upper_threshold_critical_known() const;

    /// @brief Whether Lower Threshold - Non-recoverable (0x0C) is known,
    /// i.e. not ValueUnknown. Returns false if get_entry_size() < 0x0E.
    bool is_lower_threshold_non_recoverable_known() const;

    /// @brief Whether Upper Threshold - Non-recoverable (0x0E) is known,
    /// i.e. not ValueUnknown. Returns false if get_entry_size() < 0x10.
    bool is_upper_threshold_non_recoverable_known() const;
};

} // namespace smbios
