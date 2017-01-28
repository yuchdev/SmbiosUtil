#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <smbios/smbios.h>
#include <smbios/smbios_entry_factory.h>

#define BOOST_AUTO_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Functional tests
// IMPORTANT: log level should be set to 'messages' to display in in TeamCity
// example --log_level=message

struct TimedObject
{
    typedef std::chrono::microseconds mcs;
    typedef std::chrono::system_clock clock;
    std::chrono::system_clock::time_point _timestamp;
    TimedObject() :_timestamp(clock::now()) {}
    mcs delay() { return std::chrono::duration_cast<mcs>(clock::now() - _timestamp); }
};

BOOST_AUTO_TEST_SUITE(SmbiosPerformanceTests);

// Just measure time of SMBios enumeration
BOOST_AUTO_TEST_CASE(PerformanceTestsCase)
{
    TimedObject counter;
    try {
        SMBios smbios;
        SMBiosVersion ver = smbios.get_smbios_version();
        SMBiosEntryFactory smbios_factory;
        for (const DMIHeader& header : smbios) {
            BOOST_TEST_MESSAGE("Header ID = " << header.get_type());

            std::unique_ptr<AbstractSMBiosEntry> entry = smbios_factory.create(header, ver);
            if (entry) {
                BOOST_TEST_MESSAGE("Crete entry type: " << entry->get_type());
            }
        }
    }
    catch (const std::exception& e) {
        BOOST_TEST_MESSAGE("Exception while trying to create SMBios or entry: " << e.what());
        BOOST_CHECK(false);
    }
    BOOST_TEST_MESSAGE("Total enumeration time: " << counter.delay().count() << " mcs");
}

BOOST_AUTO_TEST_SUITE_END()
