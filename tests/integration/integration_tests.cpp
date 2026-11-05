#include <vector>
#include <string>
#include <memory>
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

BOOST_AUTO_TEST_SUITE(SmbiosFunctionalTests);


/// Check consistent information after creation
BOOST_AUTO_TEST_CASE(SMBiosCreationTestCase)
{
    try{
        SMBios smbios;
        SMBiosVersion ver = smbios.get_smbios_version();
        BOOST_CHECK(ver.major_version);
        BOOST_CHECK(smbios.get_structures_count());
        BOOST_CHECK(smbios.get_structures_count());
        BOOST_CHECK(smbios.get_table_base());
        BOOST_CHECK(smbios.get_table_size());
    }
    catch (const std::exception& e){
        BOOST_TEST_MESSAGE("Exception while trying to create SMBios: " << e.what());
        BOOST_CHECK(false);
    }
}


/// Check headers iteration and any header must be non-empty
BOOST_AUTO_TEST_CASE(SMBiosIterationTestCase)
{
    try {
        SMBios smbios;
        SMBiosVersion ver = smbios.get_smbios_version();
        SMBiosEntryFactory smbios_factory;
        for (const DMIHeader& header : smbios) {
            BOOST_TEST_MESSAGE("Header ID = " << header.get_type());

            std::unique_ptr<AbstractSMBiosEntry> entry = smbios_factory.create(header, ver);
            if (entry) {
                BOOST_CHECK(entry->get_entry_size());
            }
        }
    }
    catch (const std::exception& e) {
        BOOST_TEST_MESSAGE("Exception while trying to create SMBios or entry: " << e.what());
        BOOST_CHECK(false);
    }
}


BOOST_AUTO_TEST_SUITE_END()
