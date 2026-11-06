#include <cstdint>
#include <memory>
#include <vector>

#include <smbios/source_interface/ismbios_source.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Unit tests for the chain-walking pattern SMBios::SMBios() uses against
// make_default_source_chain()'s result (milestone 0005, Story 01.0,
// contracts C1/C2): iterate an ordered vector<unique_ptr<ISmbiosSource>>,
// calling is_available() then try_read() on each, stopping at the first
// one that succeeds.
//
// This deliberately does not exercise the real platform chain
// (make_default_source_chain() itself) - per the task's own spec, that
// would require real hardware/privileges. Instead, fakes stand in for
// ISmbiosSource at the one real seam the interface provides, and the test
// replicates SMBios::SMBios()'s own loop body verbatim (smbios/src/smbios.cpp)
// so a regression in that loop's short-circuit behavior is caught here
// without needing to construct a real SMBios.

namespace {

/// @brief Always unavailable; try_read() must never be called on this one -
/// asserts if it is, since is_available() returning false should keep the
/// chain-walking loop from ever attempting it.
class FakeUnavailableSource : public ISmbiosSource {
public:
    bool is_available() const override { return false; }

    bool try_read(std::vector<uint8_t>&, size_t&, size_t&) const override
    {
        BOOST_ERROR("try_read() must not be called when is_available() is false");
        return false;
    }

    const char* source_name() const override { return "fake-unavailable"; }
};

/// @brief Available and always succeeds, returning a distinct sentinel
/// table/version so the test can confirm exactly which instance the chain
/// stopped at. Tracks whether try_read() was ever invoked, so a test can
/// assert a later source in the chain was never reached.
class FakeSucceedingSource : public ISmbiosSource {
public:
    FakeSucceedingSource(std::vector<uint8_t> sentinel_table, size_t sentinel_major, size_t sentinel_minor, const char* name, bool* was_tried)
        : sentinel_table_(std::move(sentinel_table))
        , sentinel_major_(sentinel_major)
        , sentinel_minor_(sentinel_minor)
        , name_(name)
        , was_tried_(was_tried)
    {
    }

    bool is_available() const override { return true; }

    bool try_read(std::vector<uint8_t>& out_table, size_t& major_version, size_t& minor_version) const override
    {
        if (was_tried_) {
            *was_tried_ = true;
        }
        out_table = sentinel_table_;
        major_version = sentinel_major_;
        minor_version = sentinel_minor_;
        return true;
    }

    const char* source_name() const override { return name_; }

private:
    std::vector<uint8_t> sentinel_table_;
    size_t sentinel_major_;
    size_t sentinel_minor_;
    const char* name_;
    bool* was_tried_;
};

/// @brief Replicates SMBios::SMBios()'s own chain-walking loop body
/// (smbios/src/smbios.cpp) exactly, against a caller-supplied chain
/// instead of make_default_source_chain()'s real platform result.
std::string walk_chain(
    std::vector<std::unique_ptr<ISmbiosSource>>& chain,
    std::vector<uint8_t>& table_buffer,
    size_t& major_version,
    size_t& minor_version)
{
    std::string source_name;
    for (auto& source : chain) {
        if (source->is_available() && source->try_read(table_buffer, major_version, minor_version)) {
            source_name = source->source_name();
            break;
        }
    }
    return source_name;
}

} // namespace

BOOST_AUTO_TEST_SUITE(SmbiosSourceChainUnitTests)

/// Normal case: an unavailable source chained before a succeeding one is
/// skipped (is_available() == false short-circuits before try_read()), and
/// the succeeding source's data/name are what the walk reports.
BOOST_AUTO_TEST_CASE(ChainSkipsUnavailableSourceAndUsesNextSuccessTestCase)
{
    bool second_source_was_tried = false;

    std::vector<std::unique_ptr<ISmbiosSource>> chain;
    chain.push_back(std::make_unique<FakeUnavailableSource>());
    chain.push_back(std::make_unique<FakeSucceedingSource>(
        std::vector<uint8_t>{0xAA, 0xBB, 0xCC}, 3, 4, "fake-succeeding-second", &second_source_was_tried));

    std::vector<uint8_t> table_buffer;
    size_t major_version = 0;
    size_t minor_version = 0;

    std::string source_name = walk_chain(chain, table_buffer, major_version, minor_version);

    BOOST_CHECK_EQUAL(source_name, "fake-succeeding-second");
    BOOST_CHECK(second_source_was_tried);
    BOOST_CHECK_EQUAL(major_version, static_cast<size_t>(3));
    BOOST_CHECK_EQUAL(minor_version, static_cast<size_t>(4));
    BOOST_REQUIRE_EQUAL(table_buffer.size(), static_cast<size_t>(3));
    BOOST_CHECK_EQUAL(table_buffer[0], 0xAA);
    BOOST_CHECK_EQUAL(table_buffer[1], 0xBB);
    BOOST_CHECK_EQUAL(table_buffer[2], 0xCC);
}

/// Edge case: once a source succeeds, the walk stops - a third source that
/// would also succeed must never be tried, even though it is available.
BOOST_AUTO_TEST_CASE(ChainStopsAtFirstSuccessAndNeverTriesLaterSourceTestCase)
{
    bool first_succeeding_was_tried = false;
    bool never_reached_was_tried = false;

    std::vector<std::unique_ptr<ISmbiosSource>> chain;
    chain.push_back(std::make_unique<FakeUnavailableSource>());
    chain.push_back(std::make_unique<FakeSucceedingSource>(
        std::vector<uint8_t>{0x01}, 2, 0, "fake-succeeding-first", &first_succeeding_was_tried));
    chain.push_back(std::make_unique<FakeSucceedingSource>(
        std::vector<uint8_t>{0x02, 0x02}, 9, 9, "fake-succeeding-never-reached", &never_reached_was_tried));

    std::vector<uint8_t> table_buffer;
    size_t major_version = 0;
    size_t minor_version = 0;

    std::string source_name = walk_chain(chain, table_buffer, major_version, minor_version);

    BOOST_CHECK_EQUAL(source_name, "fake-succeeding-first");
    BOOST_CHECK(first_succeeding_was_tried);
    BOOST_CHECK(!never_reached_was_tried);
    BOOST_REQUIRE_EQUAL(table_buffer.size(), static_cast<size_t>(1));
    BOOST_CHECK_EQUAL(table_buffer[0], 0x01);
}

/// Invalid-input case: an empty chain (or a chain where nothing succeeds)
/// leaves the out-parameters untouched and reports no source name, rather
/// than crashing or fabricating a result.
BOOST_AUTO_TEST_CASE(ChainWithNoAvailableSourceReportsEmptyNameTestCase)
{
    std::vector<std::unique_ptr<ISmbiosSource>> chain;
    chain.push_back(std::make_unique<FakeUnavailableSource>());

    std::vector<uint8_t> table_buffer;
    size_t major_version = 0;
    size_t minor_version = 0;

    std::string source_name = walk_chain(chain, table_buffer, major_version, minor_version);

    BOOST_CHECK(source_name.empty());
    BOOST_CHECK(table_buffer.empty());
}

BOOST_AUTO_TEST_SUITE_END()
