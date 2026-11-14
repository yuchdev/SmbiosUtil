// negproof/2: deliberate failing assertion (throwaway).
#include <boost/test/unit_test.hpp>
BOOST_AUTO_TEST_SUITE(NegproofSuite)
BOOST_AUTO_TEST_CASE(NegproofFailingAssertion) { BOOST_CHECK_EQUAL(1, 2); }
BOOST_AUTO_TEST_SUITE_END()
