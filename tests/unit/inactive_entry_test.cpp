#include <cstdint>
#include <memory>
#include <vector>

#include <libsmbios/smbios.hpp>
#include <libsmbios/tables/inactive_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>

#include "synthetic_table_builder.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, require no hardware/privileges. DSP0134 3.9.0's own 7.48
// defines no fields for this structure beyond the standard header (see
// inactive_entry.h's class doc comment) - there is nothing to vary across
// test cases beyond a single minimal-buffer factory registration/
// resolution check, per this task's own test contract (same shape as
// BootIntegrityServicesEntry's own test, Story 05.0 Task 01).

BOOST_AUTO_TEST_SUITE(InactiveEntryUnitTests)

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

/// Milestone 0010 Story 07.0 Task 03's own success criterion: Inactive
/// resolves through SMBiosEntryFactory::create() to a real InactiveEntry,
/// from a minimum-length (header-only) buffer - the only shape this
/// content-free structure ever takes.
BOOST_AUTO_TEST_CASE(CreateReturnsInactiveEntryTestCase)
{
    std::vector<uint8_t> formatted_area{};

    smbios::test::SyntheticTableBuilder builder;
    builder.add_structure(SMBios::Inactive, formatted_area);
    std::vector<uint8_t> buffer = builder.build();

    DMIHeader header = make_header(buffer);

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<InactiveEntry*>(entry.get()) != nullptr);
    BOOST_CHECK_EQUAL(entry->get_type(), "Inactive");
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
    BOOST_CHECK_THROW(InactiveEntry(header, SMBiosVersion{3, 5}, buffer.data() + buffer.size()), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
