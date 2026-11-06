#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <smbios/smbios.h>
#include <smbios/smbios_entry_factory.h>
#include <smbios/abstract_smbios_entry.h>
#include <smbios/bios_information_entry.h>
#include <smbios/memory_device_entry.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests
//
// Deterministic, no hardware/privileges needed. Two groups:
// 1. The self-registering factory machinery in isolation (smbios/include/
//    smbios/smbios_entry_factory.h), against a throwaway fake entry type
//    registered under an unused SMBiosHandler-shaped value (250, not one
//    of SMBios::SMBiosHandler's real members - see smbios.h).
// 2. Milestone 0004 Story 01.0's own success criterion ("create() returns
//    the correct concrete type for BIOSInformation, PortConnection, and
//    MemoryDevice - verified by test, not just by inspection"): PortConnection
//    is already covered through the factory in unit_tests.cpp
//    (SMBiosEntryFactoryCreatesPortConnectionEntryTestCase); the two cases
//    below close the gap for BIOSInformation/MemoryDevice, which until now
//    were only constructed directly (bios_information_entry_test.cpp/
//    memory_device_entry_test.cpp), never resolved through create() itself.

namespace {

/// @brief Unused SMBiosHandler-shaped value for this test's throwaway
/// entry type; not one of SMBios::SMBiosHandler's real members.
constexpr uint8_t kFakeHandlerValue = 250;

/// @brief Another unused value, deliberately never registered, to exercise
/// SMBiosEntryFactory::create()'s "no creator found" path.
constexpr uint8_t kUnregisteredHandlerValue = 251;

/// @brief Minimal throwaway concrete entry, used only to confirm the
/// registry/macro plumbing produces a correctly-typed instance. Not a
/// production SMBIOS structure type.
class FakeTestEntry : public AbstractSMBiosEntry {
public:
    FakeTestEntry(const DMIHeader& header, const SMBiosVersion& /*version*/, const uint8_t* table_end)
        : AbstractSMBiosEntry(header, table_end)
    {
    }

    virtual ~FakeTestEntry() = default;

    virtual std::string get_type() const override { return "Fake Test Entry"; }
    virtual std::string render_to_description() const override { return get_type(); }
};

} // namespace

// Registered at namespace scope (static-init time), exactly as a real
// entry class's own .cpp file would do - this is the only production-shaped
// use of REGISTER_SMBIOS_ENTRY in this translation unit.
REGISTER_SMBIOS_ENTRY(kFakeHandlerValue, FakeTestEntry)

BOOST_AUTO_TEST_SUITE(SMBiosEntryFactoryUnitTests)

/// Normal case: a type registered via REGISTER_SMBIOS_ENTRY resolves to a
/// correctly-typed instance through SMBiosEntryFactory::create().
BOOST_AUTO_TEST_CASE(CreateReturnsRegisteredFakeEntryTestCase)
{
    DMIHeader header{};
    header.type = kFakeHandlerValue;
    header.length = 0;
    header.handle = 0;
    header.data = nullptr;

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 0}, nullptr);

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<FakeTestEntry*>(entry.get()) != nullptr);
    BOOST_CHECK_EQUAL(entry->get_type(), "Fake Test Entry");
}

/// Edge case: a handler value nothing ever registered returns nullptr,
/// rather than throwing or default-constructing some entry.
BOOST_AUTO_TEST_CASE(CreateReturnsNullForUnregisteredHandlerValueTestCase)
{
    DMIHeader header{};
    header.type = kUnregisteredHandlerValue;
    header.length = 0;
    header.handle = 0;
    header.data = nullptr;

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 0}, nullptr);

    BOOST_CHECK(entry == nullptr);
}

/// Invalid-input case: registering the same handler_value twice is a
/// programming error and must fail fast (std::logic_error), never silently
/// overwrite the first registration. Exercised via a direct call to
/// register_smbios_entry() from an ordinary test-case body (not from a
/// static initializer), since REGISTER_SMBIOS_ENTRY itself always runs at
/// static-init time and this project's Boost.Test setup has no death-test
/// facility to catch an assert()/abort() at that point - see status.md's
/// 2026-11-07 Task 01 note for why a thrown exception was chosen over
/// assert(false) specifically so this path stays testable in-process.
BOOST_AUTO_TEST_CASE(DuplicateRegistrationThrowsTestCase)
{
    EntryCreator creator = [](const DMIHeader& h, const SMBiosVersion& v, const uint8_t* table_end) {
        return std::make_unique<FakeTestEntry>(h, v, table_end);
    };

    BOOST_CHECK_THROW(register_smbios_entry(kFakeHandlerValue, creator), std::logic_error);
}

/// @brief Minimal 4-byte structure buffer (bare header, no formatted area,
/// no strings): enough for a constructor's type check to pass and every
/// length-guarded getter to fall back to its default - sufficient to
/// confirm create() resolves to the right concrete type, which is all
/// these two cases need to prove.
std::vector<uint8_t> build_minimal_buffer(uint8_t type)
{
    return {type, 0x04, 0x01, 0x00};
}

/// Normal case: BIOSInformation resolves through create() to a real
/// BiosInformationEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsBiosInformationEntryTestCase)
{
    std::vector<uint8_t> buffer = build_minimal_buffer(SMBios::BIOSInformation);
    DMIHeader header{};
    header.type = buffer[0];
    header.length = buffer[1];
    header.handle = static_cast<uint16_t>(buffer[2] | (buffer[3] << 8));
    header.data = buffer.data();

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 0}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<BiosInformationEntry*>(entry.get()) != nullptr);
}

/// Normal case: MemoryDevice resolves through create() to a real
/// MemoryDeviceEntry, not just via direct construction.
BOOST_AUTO_TEST_CASE(CreateReturnsMemoryDeviceEntryTestCase)
{
    std::vector<uint8_t> buffer = build_minimal_buffer(SMBios::MemoryDevice);
    DMIHeader header{};
    header.type = buffer[0];
    header.length = buffer[1];
    header.handle = static_cast<uint16_t>(buffer[2] | (buffer[3] << 8));
    header.data = buffer.data();

    SMBiosEntryFactory factory;
    std::unique_ptr<AbstractSMBiosEntry> entry = factory.create(header, SMBiosVersion{2, 1}, buffer.data() + buffer.size());

    BOOST_REQUIRE(entry != nullptr);
    BOOST_CHECK(dynamic_cast<MemoryDeviceEntry*>(entry.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
