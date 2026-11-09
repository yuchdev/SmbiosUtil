// Exercises smbios_open()/smbios_close() against the real machine's
// SMBIOS table. A C++ test file calling the C API is acceptable here -
// only the API under test needs to be C, not every caller of it. Not
// wired into CMake yet (Story 03.0's job); compiled and run manually -
// see the task spec for the exact invocations used to verify it.
#include <smbios_c/smbios_c.h>

#include <cassert>
#include <cstdio>

int main()
{
    smbios_handle_t handle = nullptr;
    smbios_error_t err = smbios_open(&handle);

    if (err != SMBIOS_OK) {
        // No native SMBIOS source available on this host/under this
        // sandbox - smbios_open() must still have left the handle null,
        // which is the one invariant this test can check unconditionally.
        std::printf("smbios_open() returned error %d (no table found on this host) - handle is %s\n",
            static_cast<int>(err), handle == nullptr ? "null (correct)" : "NON-NULL (BUG)");
        assert(handle == nullptr);
        smbios_close(handle); // must be a safe no-op on nullptr
        std::printf("smbios_close(nullptr) did not crash - OK\n");
        return 0;
    }

    assert(handle != nullptr);
    std::printf("smbios_open() returned SMBIOS_OK with a non-null handle - OK\n");

    smbios_close(handle);
    std::printf("smbios_close(handle) did not crash - OK\n");

    smbios_close(nullptr);
    std::printf("smbios_close(nullptr) did not crash - OK\n");

    return 0;
}
