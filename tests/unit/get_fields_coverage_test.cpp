#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/tables/additional_information_entry.hpp>
#include <libsmbios/tables/baseboard_information_entry.hpp>
#include <libsmbios/tables/bios_information_entry.hpp>
#include <libsmbios/tables/boot_integrity_services_entry.hpp>
#include <libsmbios/tables/cache_information_entry.hpp>
#include <libsmbios/tables/cooling_device_entry.hpp>
#include <libsmbios/tables/electrical_current_probe_entry.hpp>
#include <libsmbios/tables/firmware_inventory_information_entry.hpp>
#include <libsmbios/tables/firmware_language_information_entry.hpp>
#include <libsmbios/tables/group_associations_entry.hpp>
#include <libsmbios/tables/hardware_security_entry.hpp>
#include <libsmbios/tables/inactive_entry.hpp>
#include <libsmbios/tables/ipmi_device_information_entry.hpp>
#include <libsmbios/tables/management_controller_host_interface_entry.hpp>
#include <libsmbios/tables/management_device_component_entry.hpp>
#include <libsmbios/tables/management_device_entry.hpp>
#include <libsmbios/tables/management_device_threshold_data_entry.hpp>
#include <libsmbios/tables/memory_array_mapped_address_entry.hpp>
#include <libsmbios/tables/memory_channel_entry.hpp>
#include <libsmbios/tables/memory_device_entry.hpp>
#include <libsmbios/tables/memory_device_mapped_address_entry.hpp>
#include <libsmbios/tables/memory_error_information_32bit_entry.hpp>
#include <libsmbios/tables/memory_error_information_64bit_entry.hpp>
#include <libsmbios/tables/oem_strings_entry.hpp>
#include <libsmbios/tables/onboard_devices_extended_information_entry.hpp>
#include <libsmbios/tables/out_of_band_remote_access_entry.hpp>
#include <libsmbios/tables/physical_memory_array_entry.hpp>
#include <libsmbios/tables/pointing_device_entry.hpp>
#include <libsmbios/tables/port_connection_entry.hpp>
#include <libsmbios/tables/portable_battery_entry.hpp>
#include <libsmbios/tables/processor_additional_information_entry.hpp>
#include <libsmbios/tables/processor_information_entry.hpp>
#include <libsmbios/tables/string_property_entry.hpp>
#include <libsmbios/tables/system_boot_information_entry.hpp>
#include <libsmbios/tables/system_configuration_options_entry.hpp>
#include <libsmbios/tables/system_enclosure_entry.hpp>
#include <libsmbios/tables/system_event_log_entry.hpp>
#include <libsmbios/tables/system_information_entry.hpp>
#include <libsmbios/tables/system_power_controls_entry.hpp>
#include <libsmbios/tables/system_power_supply_entry.hpp>
#include <libsmbios/tables/system_reset_entry.hpp>
#include <libsmbios/tables/system_slots_entry.hpp>
#include <libsmbios/tables/temperature_probe_entry.hpp>
#include <libsmbios/tables/tpm_device_entry.hpp>
#include <libsmbios/tables/voltage_probe_entry.hpp>

#include "synthetic_table_builder.hpp"
#include "field_assertions.hpp"
#include "formatter_test_support.hpp"

#include <boost/test/unit_test.hpp>

using namespace smbios;

// Unit tests (milestone 0009, Story 01.0, Task 05)
//
// Type: unit - deterministic, no hardware/privileges.
// Scenario: every one of the 45 SMBIOS structure types registered through
// REGISTER_SMBIOS_ENTRY resolves through SMBiosEntryFactory::create() to its
// concrete class, and that class's get_fields() (pure virtual since this
// task) is callable and yields unique field names. InactiveEntry and
// BootIntegrityServicesEntry yield an empty vector (their text output has
// no data fields); every other type yields at least one field. Story 02.0
// Task 03 adds FieldNamesAreFirmwareIndependent (security case 10): the
// name sequence does not change when every DMI string is hostile.
// Boundaries: one well-formed structure per type, its formatted area filled
// with 0x02 bytes (the conventional SMBIOS "Unknown" enum code, and a valid
// string index into the two strings supplied) and long enough (0x64) to
// reach every length tier of every type, at SMBIOS 3.9 so every version
// tier is in play.
// On failure first check: the failing type's get_fields() in
// libsmbios/src/<snake>_entry.cpp; for a nullptr from create(), that its
// REGISTER_SMBIOS_ENTRY line is present and its object file is linked
// (this TU names every class below via dynamic_cast for exactly that
// reason - smbios_unit_test does not force-load libsmbios).

BOOST_AUTO_TEST_SUITE(GetFieldsCoverageUnitTests)

namespace {

/// @brief One registered type: its handler value, its class name for
/// messages, and a dynamic_cast check naming the concrete class (which also
/// forces the class's object file, and so its registration, to be linked).
struct CoverageCase {
    uint8_t type;
    const char* class_name;
    bool (*is_expected_class)(const AbstractSMBiosEntry*);
};

#define COVERAGE_CASE(handler, class_name) \
    CoverageCase{ static_cast<uint8_t>(SMBios::handler), #class_name, \
                  [](const AbstractSMBiosEntry* entry) { return dynamic_cast<const class_name*>(entry) != nullptr; } }

std::vector<CoverageCase> all_registered_types()
{
    return {
        COVERAGE_CASE(BIOSInformation, BiosInformationEntry),
        COVERAGE_CASE(SystemInformation, SystemInformationEntry),
        COVERAGE_CASE(BaseboardInformation, BaseboardInformationEntry),
        COVERAGE_CASE(SystemEnclosure, SystemEnclosureEntry),
        COVERAGE_CASE(ProcessorInformation, ProcessorInformationEntry),
        COVERAGE_CASE(CacheInformation, CacheInformationEntry),
        COVERAGE_CASE(PortConnection, PortConnectionEntry),
        COVERAGE_CASE(SystemSlots, SystemSlotsEntry),
        COVERAGE_CASE(OemStrings, OemStringsEntry),
        COVERAGE_CASE(SystemConfigurationOptions, SystemConfigurationOptionsEntry),
        COVERAGE_CASE(FirmwareLanguageInformation, FirmwareLanguageInformationEntry),
        COVERAGE_CASE(GroupAssociations, GroupAssociationsEntry),
        COVERAGE_CASE(SystemEventLog, SystemEventLogEntry),
        COVERAGE_CASE(PhysicalMemoryArray, PhysicalMemoryArrayEntry),
        COVERAGE_CASE(MemoryDevice, MemoryDeviceEntry),
        COVERAGE_CASE(MemoryErrorInformation32Bit, MemoryErrorInformation32BitEntry),
        COVERAGE_CASE(MemoryArrayMappedAddress, MemoryArrayMappedAddressEntry),
        COVERAGE_CASE(MemoryDeviceMappedAddress, MemoryDeviceMappedAddressEntry),
        COVERAGE_CASE(PointingDevice, PointingDeviceEntry),
        COVERAGE_CASE(PortableBattery, PortableBatteryEntry),
        COVERAGE_CASE(SystemReset, SystemResetEntry),
        COVERAGE_CASE(HardwareSecurity, HardwareSecurityEntry),
        COVERAGE_CASE(SystemPowerControls, SystemPowerControlsEntry),
        COVERAGE_CASE(VoltageProbe, VoltageProbeEntry),
        COVERAGE_CASE(CoolingDevice, CoolingDeviceEntry),
        COVERAGE_CASE(TemperatureProbe, TemperatureProbeEntry),
        COVERAGE_CASE(ElectricalCurrentProbe, ElectricalCurrentProbeEntry),
        COVERAGE_CASE(OutOfBandRemoteAccess, OutOfBandRemoteAccessEntry),
        COVERAGE_CASE(BootIntegrityServices, BootIntegrityServicesEntry),
        COVERAGE_CASE(SystemBootInformation, SystemBootInformationEntry),
        COVERAGE_CASE(MemoryErrorInformation64Bit, MemoryErrorInformation64BitEntry),
        COVERAGE_CASE(ManagementDevice, ManagementDeviceEntry),
        COVERAGE_CASE(ManagementDeviceComponent, ManagementDeviceComponentEntry),
        COVERAGE_CASE(ManagementDeviceThresholdData, ManagementDeviceThresholdDataEntry),
        COVERAGE_CASE(MemoryChannel, MemoryChannelEntry),
        COVERAGE_CASE(IpmiDeviceInformation, IpmiDeviceInformationEntry),
        COVERAGE_CASE(SystemPowerSupply, SystemPowerSupplyEntry),
        COVERAGE_CASE(AdditionalInformation, AdditionalInformationEntry),
        COVERAGE_CASE(OnboardDevicesExtendedInformation, OnboardDevicesExtendedInformationEntry),
        COVERAGE_CASE(ManagementControllerHostInterface, ManagementControllerHostInterfaceEntry),
        COVERAGE_CASE(TpmDevice, TpmDeviceEntry),
        COVERAGE_CASE(ProcessorAdditionalInformation, ProcessorAdditionalInformationEntry),
        COVERAGE_CASE(FirmwareInventoryInformation, FirmwareInventoryInformationEntry),
        COVERAGE_CASE(StringProperty, StringPropertyEntry),
        COVERAGE_CASE(Inactive, InactiveEntry),
    };
}

#undef COVERAGE_CASE

/// Formatted-area length (header excluded): total structure length 0x64,
/// at least as long as every type's longest DSP0134 3.9.0 layout.
constexpr size_t kFormattedAreaLength = 0x60;

/// @brief Builds one `type` structure (0x02-filled formatted area, so every
/// string index selects string 2) with `strings` and returns its ordered
/// get_fields() names, after checking they are unique.
std::vector<std::string> field_names_for(uint8_t type, const std::vector<std::string>& strings)
{
    const std::vector<uint8_t> buffer =
        smbios::test::SyntheticTableBuilder().add_structure(type, std::vector<uint8_t>(kFormattedAreaLength, 0x02), strings).build();
    std::unique_ptr<AbstractSMBiosEntry> entry = SMBiosEntryFactory().create(
        smbios::test::make_header(buffer), SMBiosVersion{3, 9}, buffer.data() + buffer.size());
    BOOST_REQUIRE(entry != nullptr);
    const std::vector<NamedField> fields = entry->get_fields();
    smbios::test::check_unique_field_names(fields);
    std::vector<std::string> names;
    for (const NamedField& field : fields) {
        names.push_back(field.name);
    }
    return names;
}

} // namespace

/// Sanity: the table above covers exactly the 45 registered types, each
/// type value once.
BOOST_AUTO_TEST_CASE(CoverageTableListsAll45RegisteredTypesTestCase)
{
    const std::vector<CoverageCase> cases = all_registered_types();
    BOOST_CHECK_EQUAL(cases.size(), static_cast<size_t>(45));
    std::vector<bool> seen(256, false);
    for (const auto& coverage_case : cases) {
        BOOST_CHECK_MESSAGE(!seen[coverage_case.type], "duplicate type " << static_cast<unsigned>(coverage_case.type));
        seen[coverage_case.type] = true;
    }
}

/// Normal case, all 45 types: create() returns the concrete class, and its
/// get_fields() yields unique names - empty for the two field-less types,
/// non-empty for every other.
BOOST_AUTO_TEST_CASE(GetFieldsCoversEveryRegisteredTypeTestCase)
{
    using namespace smbios::test;
    SMBiosEntryFactory factory;
    for (const auto& coverage_case : all_registered_types()) {
        BOOST_TEST_CONTEXT("type " << static_cast<unsigned>(coverage_case.type) << " (" << coverage_case.class_name << ")") {
            SyntheticTableBuilder builder;
            builder.add_structure(coverage_case.type, std::vector<uint8_t>(kFormattedAreaLength, 0x02),
                                  {"String One", "String Two"});
            std::vector<uint8_t> buffer = builder.build();

            std::unique_ptr<AbstractSMBiosEntry> entry =
                factory.create(smbios::test::make_header(buffer), SMBiosVersion{3, 9}, buffer.data() + buffer.size());
            BOOST_REQUIRE(entry != nullptr);
            BOOST_CHECK(coverage_case.is_expected_class(entry.get()));

            const std::vector<NamedField> fields = entry->get_fields();
            check_unique_field_names(fields);
            if (coverage_case.type == SMBios::Inactive || coverage_case.type == SMBios::BootIntegrityServices) {
                BOOST_CHECK(fields.empty());
            }
            else {
                BOOST_CHECK(!fields.empty());
            }
        }
    }
}

/// Security case (Task 03 case 10, INFO-6 / CWE-91): for all 45 registered
/// types, the ordered get_fields() name sequence is identical whether every
/// DMI string is "A" or one of the case-1 invalid-UTF-8 values - names are
/// library constants, never derived from firmware text - each name is
/// printable ASCII (0x20-0x7E), and names stay unique under hostile
/// strings, so map-shaped formats never see a duplicate key.
/// On failure: the type's get_fields() is building a name from a string
/// value (or from string content, e.g. its length).
BOOST_AUTO_TEST_CASE(FieldNamesAreFirmwareIndependent)
{
    for (const auto& coverage_case : all_registered_types()) {
        BOOST_TEST_CONTEXT("type " << static_cast<unsigned>(coverage_case.type) << " (" << coverage_case.class_name << ")") {
            const std::vector<std::string> baseline = field_names_for(coverage_case.type, {"A", "A"});
            for (const std::string& name : baseline) {
                BOOST_TEST_CONTEXT("name \"" << name << '"') {
                    BOOST_CHECK(!name.empty());
                    for (const char c : name) {
                        const auto b = static_cast<unsigned char>(c);
                        BOOST_CHECK(b >= 0x20 && b <= 0x7E);
                    }
                }
            }
            for (const auto& hostile : smbios::test::invalid_utf8_cases()) {
                BOOST_TEST_CONTEXT("hostile string of " << hostile.first.size() << " bytes") {
                    const std::vector<std::string> names =
                        field_names_for(coverage_case.type, {hostile.first, hostile.first});
                    BOOST_CHECK_EQUAL_COLLECTIONS(names.begin(), names.end(), baseline.begin(), baseline.end());
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
