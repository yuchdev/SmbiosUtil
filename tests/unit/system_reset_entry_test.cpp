#include <cstdint>
#include <string>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/system_reset_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>

#include "synthetic_table_builder.hpp"
#include "field_assertions.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges: synthetic buffers (via
// smbios::test::SyntheticTableBuilder) drive SystemResetEntry
// (smbios/include/smbios/tables/system_reset_entry.hpp), cross-checked
// against DSP0134 3.9.0's own 7.24 (Table 92), whose Capabilities row was
// redacted in this task's own source excerpt and was therefore re-confirmed
// directly against docs/standard/DSP0134_3.9.0.md before writing any
// decoding logic here:
//   Bit 0 Status, Bits 2:1 Boot Option, Bits 4:3 Boot Option on Limit,
//   Bit 5 Watchdog present, Bits 7:6 Reserved.
//
// Wire layout (formatted area, offsets relative to structure start):
//   offset 0x04 : uint8_t  capabilities
//   offset 0x05 : uint16_t reset_count
//   offset 0x07 : uint16_t reset_limit
//   offset 0x09 : uint16_t timer_interval
//   offset 0x0B : uint16_t timeout

BOOST_AUTO_TEST_SUITE(SystemResetEntryUnitTests)

namespace {

DMIHeader make_header(const std::vector<uint8_t>& buffer)
{
    DMIHeader header{};
    header.type = buffer[0];
    header.length = buffer[1];
    header.handle = static_cast<uint16_t>(buffer[2] | (buffer[3] << 8));
    header.data = buffer.data();
    return header;
}

/// Builds a full (0x0D-byte header.length) System Reset formatted area.
std::vector<uint8_t> build_formatted_area(uint8_t capabilities, uint16_t reset_count,
                                           uint16_t reset_limit, uint16_t timer_interval,
                                           uint16_t timeout)
{
    return std::vector<uint8_t>{
        capabilities, // 0x04
        static_cast<uint8_t>(reset_count & 0xFF), static_cast<uint8_t>((reset_count >> 8) & 0xFF), // 0x05-0x06
        static_cast<uint8_t>(reset_limit & 0xFF), static_cast<uint8_t>((reset_limit >> 8) & 0xFF), // 0x07-0x08
        static_cast<uint8_t>(timer_interval & 0xFF), static_cast<uint8_t>((timer_interval >> 8) & 0xFF), // 0x09-0x0A
        static_cast<uint8_t>(timeout & 0xFF), static_cast<uint8_t>((timeout >> 8) & 0xFF), // 0x0B-0x0C
    };
}

} // namespace

/// Normal case: every documented Boot Option / Boot Option on Limit value
/// (Table 92's shared 2-bit space), with Status and Watchdog both set.
BOOST_AUTO_TEST_CASE(EveryBootOptionValueDecodesTestCase)
{
    const std::vector<std::pair<SystemResetEntry::BootOption, std::string>> cases{
        {SystemResetEntry::Reserved, "Reserved"},
        {SystemResetEntry::OperatingSystem, "Operating system"},
        {SystemResetEntry::SystemUtilities, "System utilities"},
        {SystemResetEntry::DoNotReboot, "Do not reboot"},
    };

    for (const auto& boot_option_case : cases) {
        // Bit 0 (Status) = 1, Bit 5 (Watchdog) = 1, Bits 2:1 = Bits 4:3 =
        // boot_option_case.first.
        const uint8_t capabilities = static_cast<uint8_t>(
            0x01 | 0x20 |
            (boot_option_case.first << 1) |
            (boot_option_case.first << 3));

        std::vector<uint8_t> formatted_area = build_formatted_area(capabilities, 1, 2, 3, 4);

        smbios::test::SyntheticTableBuilder builder;
        builder.add_structure(SMBios::SystemReset, formatted_area);
        std::vector<uint8_t> buffer = builder.build();

        DMIHeader header = make_header(buffer);
        SystemResetEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

        BOOST_CHECK_EQUAL(entry.get_boot_option(), boot_option_case.first);
        BOOST_CHECK_EQUAL(entry.get_boot_option_string(), boot_option_case.second);
        BOOST_CHECK_EQUAL(entry.get_boot_option_on_limit(), boot_option_case.first);
        BOOST_CHECK_EQUAL(entry.get_boot_option_on_limit_string(), boot_option_case.second);
        BOOST_CHECK(entry.is_status_enabled());
        BOOST_CHECK(entry.is_watchdog_present());
    }
}

/// Normal case: Status disabled, Watchdog not present, distinct Boot
/// Option vs Boot Option on Limit values, to confirm the two 2-bit fields
/// are decoded independently (not aliased).
BOOST_AUTO_TEST_CASE(StatusDisabledWatchdogAbsentDistinctBootOptionsTestCase)
{
    // Bit 0 = 0, Bit 5 = 0, Bits 2:1 = OperatingSystem (01b), Bits 4:3 =
    // DoNotReboot (11b).
    const uint8_t capabilities = static_cast<uint8_t>(
        (SystemResetEntry::OperatingSystem << 1) |
        (SystemResetEntry::DoNotReboot << 3));

    std::vector<uint8_t> formatted_area = build_formatted_area(capabilities, 1, 2, 3, 4);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemResetEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_status_enabled());
    BOOST_CHECK(!entry.is_watchdog_present());
    BOOST_CHECK_EQUAL(entry.get_boot_option(), SystemResetEntry::OperatingSystem);
    BOOST_CHECK_EQUAL(entry.get_boot_option_on_limit(), SystemResetEntry::DoNotReboot);
}

/// Distinct unknown-sentinel case: Reset Count == 0xFFFF -> unknown.
BOOST_AUTO_TEST_CASE(ResetCountFFFFIsUnknownTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(0x01, SystemResetEntry::ValueUnknown, 2, 3, 4);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemResetEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_reset_count_known());
    BOOST_CHECK_EQUAL(entry.get_reset_count(), SystemResetEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Reset Limit == 0xFFFF -> unknown.
BOOST_AUTO_TEST_CASE(ResetLimitFFFFIsUnknownTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(0x01, 1, SystemResetEntry::ValueUnknown, 3, 4);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemResetEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_reset_limit_known());
    BOOST_CHECK_EQUAL(entry.get_reset_limit(), SystemResetEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Timer Interval == 0xFFFF -> unknown.
BOOST_AUTO_TEST_CASE(TimerIntervalFFFFIsUnknownTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(0x01, 1, 2, SystemResetEntry::ValueUnknown, 4);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemResetEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_timer_interval_known());
    BOOST_CHECK_EQUAL(entry.get_timer_interval(), SystemResetEntry::ValueUnknown);
}

/// Distinct unknown-sentinel case: Timeout == 0xFFFF -> unknown.
BOOST_AUTO_TEST_CASE(TimeoutFFFFIsUnknownTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(0x01, 1, 2, 3, SystemResetEntry::ValueUnknown);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemResetEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(!entry.is_timeout_known());
    BOOST_CHECK_EQUAL(entry.get_timeout(), SystemResetEntry::ValueUnknown);
}

/// Normal case: known, non-sentinel values round-trip exactly.
BOOST_AUTO_TEST_CASE(KnownCounterValuesDecodeTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(0x01, 5, 10, 15, 20);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemResetEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK(entry.is_reset_count_known());
    BOOST_CHECK_EQUAL(entry.get_reset_count(), static_cast<uint16_t>(5));
    BOOST_CHECK(entry.is_reset_limit_known());
    BOOST_CHECK_EQUAL(entry.get_reset_limit(), static_cast<uint16_t>(10));
    BOOST_CHECK(entry.is_timer_interval_known());
    BOOST_CHECK_EQUAL(entry.get_timer_interval(), static_cast<uint16_t>(15));
    BOOST_CHECK(entry.is_timeout_known());
    BOOST_CHECK_EQUAL(entry.get_timeout(), static_cast<uint16_t>(20));
}

/// Edge case: minimum-length buffer (header.length == 0x04, no formatted
/// fields at all) -> every getter defaults (0 capabilities, unknown
/// sentinels).
BOOST_AUTO_TEST_CASE(MinimumLengthBufferDefaultsEveryFieldTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    SystemResetEntry entry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_CHECK_EQUAL(entry.get_capabilities(), static_cast<uint8_t>(0));
    BOOST_CHECK(!entry.is_status_enabled());
    BOOST_CHECK(!entry.is_watchdog_present());
    BOOST_CHECK_EQUAL(entry.get_boot_option(), SystemResetEntry::Reserved);
    BOOST_CHECK_EQUAL(entry.get_boot_option_on_limit(), SystemResetEntry::Reserved);
    BOOST_CHECK(!entry.is_reset_count_known());
    BOOST_CHECK_EQUAL(entry.get_reset_count(), SystemResetEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_reset_limit_known());
    BOOST_CHECK_EQUAL(entry.get_reset_limit(), SystemResetEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_timer_interval_known());
    BOOST_CHECK_EQUAL(entry.get_timer_interval(), SystemResetEntry::ValueUnknown);
    BOOST_CHECK(!entry.is_timeout_known());
    BOOST_CHECK_EQUAL(entry.get_timeout(), SystemResetEntry::ValueUnknown);
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(0x01, 1, 2, 3, 4);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::HardwareSecurity, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(SystemResetEntry(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size()), std::runtime_error);
}

/// Milestone 0010 Story 04.0 Task 02's own success criterion: SystemReset
/// resolves through SMBiosEntryFactory::create() to a real
/// SystemResetEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsSystemResetEntryTestCase)
{
    std::vector<uint8_t> formatted_area = build_formatted_area(0x01, 1, 2, 3, 4);

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<SystemResetEntry*>(entry.get()) != nullptr);
}

// get_fields() (milestone 0009, Story 01.0, Task 04)
//
// Type: unit - deterministic, hand-built buffers, no hardware/privileges.
// Scenario: get_fields() mirrors render_to_description() field by field;
// Status (Enabled/Disabled) and Watchdog Timer (Present/Not Present) are
// bool, boot options stay strings, and the counters/intervals stay the
// printed string because each may print "Unknown". No bitmask list in this
// type (Capabilities is two flags and two 2-bit enumerations).
// Boundaries: all counters at the 0xFFFF "Unknown" sentinel, a bare header.
// On failure first check: SystemResetEntry::get_fields() in
// libsmbios/src/system_reset_entry.cpp.

/// Normal case: every text label becomes a field, in text order.
BOOST_AUTO_TEST_CASE(SystemResetEntryGetFieldsMatchesDecodedValuesTestCase)
{
    using namespace smbios::test;
    // Status enabled (bit 0), boot option = OS (bits 2:1 = 1), on limit =
    // System utilities (bits 4:3 = 2), watchdog present (bit 5).
    std::vector<uint8_t> formatted_area = build_formatted_area(0x33, 3, 5, 10, 20);
    SyntheticTableBuilder builder;
    builder.add_structure(SMBios::SystemReset, formatted_area);
    std::vector<uint8_t> buffer = builder.build();
    SystemResetEntry entry(make_header(buffer), SMBiosVersion{2, 2}, buffer.data() + buffer.size());

    check_fields_equal(entry.get_fields(), {
        {"Status", true},
        {"Watchdog Timer", true},
        {"Boot Option", str("Operating system")},
        {"Boot Option on Limit", str("System utilities")},
        {"Reset Count", str("3")},
        {"Reset Limit", str("5")},
        {"Timer Interval", str("10 min")},
        {"Timeout", str("20 min")},
    });
}

/// Boundary cases: the 0xFFFF sentinel prints "Unknown" for every counter;
/// a bare header keeps every field (the text prints every line
/// unconditionally) with the getters' defaults.
BOOST_AUTO_TEST_CASE(SystemResetEntryGetFieldsHonoursLengthTiersTestCase)
{
    using namespace smbios::test;
    {
        std::vector<uint8_t> formatted_area = build_formatted_area(0x00, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF);
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::SystemReset, formatted_area);
        std::vector<uint8_t> buffer = builder.build();
        SystemResetEntry entry(make_header(buffer), SMBiosVersion{2, 2}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Status", false},
            {"Watchdog Timer", false},
            {"Boot Option", str("Reserved")},
            {"Boot Option on Limit", str("Reserved")},
            {"Reset Count", str("Unknown")},
            {"Reset Limit", str("Unknown")},
            {"Timer Interval", str("Unknown")},
            {"Timeout", str("Unknown")},
        });
    }
    {
        SyntheticTableBuilder builder;
        builder.add_structure(SMBios::SystemReset, std::vector<uint8_t>{});
        std::vector<uint8_t> buffer = builder.build();
        SystemResetEntry entry(make_header(buffer), SMBiosVersion{2, 2}, buffer.data() + buffer.size());

        check_fields_equal(entry.get_fields(), {
            {"Status", false},
            {"Watchdog Timer", false},
            {"Boot Option", str("Reserved")},
            {"Boot Option on Limit", str("Reserved")},
            {"Reset Count", str("Unknown")},
            {"Reset Limit", str("Unknown")},
            {"Timer Interval", str("Unknown")},
            {"Timeout", str("Unknown")},
        });
    }
}

BOOST_AUTO_TEST_SUITE_END()
