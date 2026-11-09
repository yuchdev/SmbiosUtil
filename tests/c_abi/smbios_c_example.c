/* End-to-end example proving smbios_c is genuinely usable from a plain C
 * program, not just from C++ test files pretending to be C (the other
 * tests in this directory, smbios_open_close_test.cpp and friends, call
 * the API from C++). Compiled under a real C compiler as part of Story
 * 02.0 Task 03's verification - see status.md for the exact command and
 * output. Not wired into CMake yet (Story 03.0's job). */
#include <smbios/smbios.h>

#include <stdio.h>
#include <stdlib.h>

static void render_entry(smbios_entry_handle_t entry)
{
    size_t needed = 0;
    smbios_error_t err = smbios_entry_render(entry, NULL, 0, &needed);
    if (err != SMBIOS_ERR_BUFFER_TOO_SMALL) {
        fprintf(stderr, "smbios_entry_render() size query failed: %d\n", (int)err);
        return;
    }

    char* buf = malloc(needed);
    if (buf == NULL) {
        return;
    }

    err = smbios_entry_render(entry, buf, needed, &needed);
    if (err == SMBIOS_OK) {
        printf("%s\n", buf);
    }
    free(buf);
}

int main(void)
{
    smbios_handle_t handle = NULL;
    smbios_error_t err = smbios_open(&handle);
    if (err != SMBIOS_OK) {
        printf("smbios_open() failed with error %d (no SMBIOS table on this host)\n", (int)err);
        return 0;
    }

    size_t count = 0;
    err = smbios_entry_count(handle, &count);
    if (err != SMBIOS_OK) {
        fprintf(stderr, "smbios_entry_count() failed: %d\n", (int)err);
        smbios_close(handle);
        return 1;
    }

    printf("SMBIOS table has %zu structures\n", count);

    for (size_t i = 0; i < count; ++i) {
        smbios_entry_handle_t entry = NULL;
        if (smbios_entry_at(handle, i, &entry) != SMBIOS_OK) {
            continue;
        }

        uint8_t type = smbios_entry_type(entry);
        printf("[%zu] type=%u length=%u handle=%u: ", i, (unsigned)type,
            (unsigned)smbios_entry_length(entry), (unsigned)smbios_entry_handle_id(entry));
        render_entry(entry);

        if (type == 0) { /* BIOSInformation */
            char vendor[256];
            size_t needed = 0;
            if (smbios_bios_vendor(entry, vendor, sizeof(vendor), &needed) == SMBIOS_OK) {
                printf("  BIOS vendor: %s\n", vendor);
            }
        }
    }

    smbios_close(handle);
    return 0;
}
