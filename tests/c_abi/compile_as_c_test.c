/* Proves smbios_c.h is genuinely C-compatible, not just C++ with
 * extern "C" that happens to also parse under a C compiler. Not wired
 * into CMake yet - Story 03.0 adds the build integration; until then
 * this file is compiled manually (see the task spec for the exact
 * gcc/clang invocations used to verify it). */
#include <smbios_c/smbios_c.h>

int main(void)
{
    smbios_handle_t handle = NULL;
    smbios_error_t err = SMBIOS_OK;

    (void)handle;
    (void)err;

    return 0;
}
