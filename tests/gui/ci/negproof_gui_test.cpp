// negproof/3: deliberate failing gui-ci assertion (throwaway).
#include <boost/test/unit_test.hpp>
BOOST_AUTO_TEST_SUITE(NegproofGuiSuite)
BOOST_AUTO_TEST_CASE(NegproofGuiFailingAssertion) { BOOST_CHECK_EQUAL(1, 2); }
BOOST_AUTO_TEST_SUITE_END()
