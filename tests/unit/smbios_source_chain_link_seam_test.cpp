#include <memory>
#include <vector>

#include <smbios/sys_interface/smbios_source_chain.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/test/auto_unit_test.hpp>

using namespace boost::unit_test;
using namespace smbios;

// Link-seam proof for milestone 0005, Story 01.1, Task 03: confirms that
// linking smbios (core) alone is sufficient to resolve
// smbios::make_default_source_chain() with no unresolved symbol - the
// concrete proof that this story's "link-seam instead of preprocessor-seam"
// resolution of R8 actually works, on whichever platform CI runs this
// suite on.
//
// smbios_source_chain.h only *declares* make_default_source_chain(); its
// *definition* lives in exactly one of smbios_windows/smbios_macos/smbios_linux
// (or fallback_source_chain.cpp on BSD/Solaris) - whichever one
// smbios/src/CMakeLists.txt's if(WIN32)/elseif(APPLE)/elseif(Linux)/else()
// branch selected at configure time (see that file's own comment). This
// test does not link any per-OS library directly (per this task's own
// constraint) - it links only smbios_unit_test's existing smbios
// dependency, the same way tests/integration/ already does. If
// make_default_source_chain() were ever left undefined on some platform's
// branch, this test's own link step - not its assertion - is where that
// regression would first surface.
//
// This is a link-seam proof, not a behavior test: it does not assert
// anything about which concrete ISmbiosSource(s) the chain contains, only
// that resolving the symbol and calling it yields a non-empty chain. Real
// platform-specific behavior is covered by tests/integration/ and
// tests/perf/, which construct a real SMBios end-to-end.

BOOST_AUTO_TEST_SUITE(SmbiosSourceChainLinkSeamUnitTests)

/// Normal case: make_default_source_chain() resolves through smbios core's
/// link graph with no unresolved symbol, and returns at least one source on
/// every platform this milestone currently supports (Windows/macOS/Linux
/// per-OS libraries, or the BSD/Solaris fallback).
BOOST_AUTO_TEST_CASE(MakeDefaultSourceChainResolvesAndIsNonEmptyTestCase)
{
    BOOST_CHECK(!make_default_source_chain().empty());
}

BOOST_AUTO_TEST_SUITE_END()
