#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/boot_integrity_services_entry.hpp>
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
// Deterministic, require no hardware/privileges. DSP0134 3.9.0's own 7.32
// defines no fields for this structure (see
// boot_integrity_services_entry.hpp's class doc comment) - there is nothing
// to vary across test cases beyond a single minimal-buffer factory
// registration/resolution check, per this task's own test contract.

BOOST_AUTO_TEST_SUITE(BootIntegrityServicesEntryUnitTests)

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

} // namespace

/// Milestone 0010 Story 05.0 Task 01's own success criterion:
/// BootIntegrityServices resolves through SMBiosEntryFactory::create() to a
/// real BootIntegrityServicesEntry, from a minimum-length (header-only)
/// buffer - the only shape this content-free structure ever takes.
BOOST_AUTO_TEST_CASE(CreateReturnsBootIntegrityServicesEntryTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::BootIntegrityServices, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<BootIntegrityServicesEntry*>(entry.get()) != nullptr);
    BOOST_CHECK_EQUAL(entry->get_type(), "Boot Integrity Services Entry Point");
    BOOST_CHECK(!entry->render_to_description().empty());
}

/// Invalid-input case: wrong structure type passed to the constructor
/// throws, rather than silently misinterpreting the buffer.
BOOST_AUTO_TEST_CASE(WrongTypeThrowsTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::HardwareSecurity, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);
    BOOST_CHECK_THROW(BootIntegrityServicesEntry(header, SMBiosVersion{2, 3}, buffer.data() + buffer.size()), std::runtime_error);
}

// get_fields() (milestone 0009 Story 01.0 Task 05)
//
// Type: unit - deterministic, no hardware/privileges.
// Scenario: get_fields() is deliberately empty, like InactiveEntry: the
// text prints only the header type and a fixed sentence.
// Boundaries: the header-only shape and a structure with trailing bytes
// (still nothing decoded). No bitmask and no length tiers.
// On failure first check: BootIntegrityServicesEntry::get_fields() against
// BootIntegrityServicesEntry::render_to_description() in libsmbios/src/boot_integrity_services_entry.cpp.

/// Normal case: no DSP0134-defined fields, so no structured fields - even
/// when the structure carries trailing bytes.
BOOST_AUTO_TEST_CASE(BootIntegrityServicesEntryGetFieldsMatchesDecodedValuesTestCase)
{
    for (const std::vector<uint8_t>& formatted_area : {std::vector<uint8_t>{}, std::vector<uint8_t>{0x01, 0x02, 0x03}}) {
        smbios::test::SyntheticTableBuilder builder;
        builder.add_structure(SMBios::BootIntegrityServices, formatted_area);
        std::vector<uint8_t> buffer = builder.build();
        BootIntegrityServicesEntry entry(make_header(buffer), SMBiosVersion{2, 3}, buffer.data() + buffer.size());

        BOOST_CHECK(entry.get_fields().empty());
    }
}

BOOST_AUTO_TEST_SUITE_END()
