// Exercises entry iteration and rendering against the real machine's
// SMBIOS table, cross-checked byte-for-byte against the equivalent
// direct C++ calls (SMBiosEntryFactory::create() + render_to_description()
// - the same pattern smbios/src/main.cpp itself uses). Not wired into
// CMake yet (Story 03.0's job); compiled and run manually.
#include <smbios_c/smbios_c.h>

#include <libsmbios/smbios.h>
#include <libsmbios/abstract_smbios_entry.h>
#include <libsmbios/smbios_entry_factory.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

int main()
{
    smbios_handle_t handle = nullptr;
    smbios_error_t err = smbios_open(&handle);
    if (err != SMBIOS_OK) {
        std::printf("smbios_open() returned error %d - no table on this host, skipping iteration checks\n",
            static_cast<int>(err));
        return 0;
    }

    // Reference walk: a second, independent smbios::SMBios + the same
    // factory.create()/render_to_description() calls main.cpp makes -
    // the C API's output must match this exactly, entry for entry.
    smbios::SMBios reference_bios;
    smbios::SMBiosEntryFactory factory;
    const uint8_t* table_end = reference_bios.get_table_base() + reference_bios.get_table_size();

    std::vector<std::string> reference_descriptions;
    for (const smbios::DMIHeader& header : reference_bios) {
        std::unique_ptr<smbios::AbstractSMBiosEntry> entry = factory.create(header, reference_bios.get_smbios_version(), table_end);
        reference_descriptions.push_back(entry ? entry->render_to_description()
                                                 : ("Unknown structure type " + std::to_string(static_cast<unsigned>(header.type))));
    }

    size_t count = 0;
    err = smbios_entry_count(handle, &count);
    assert(err == SMBIOS_OK);
    std::printf("smbios_entry_count() = %zu, reference count = %zu\n", count, reference_descriptions.size());
    assert(count == reference_descriptions.size());

    for (size_t i = 0; i < count; ++i) {
        smbios_entry_handle_t entry = nullptr;
        err = smbios_entry_at(handle, i, &entry);
        assert(err == SMBIOS_OK);
        assert(entry != nullptr);

        // First call with a zero-length buffer: must report
        // SMBIOS_ERR_BUFFER_TOO_SMALL and the exact required size.
        size_t needed_len = 0;
        smbios_error_t render_err = smbios_entry_render(entry, nullptr, 0, &needed_len);
        assert(render_err == SMBIOS_ERR_BUFFER_TOO_SMALL);
        assert(needed_len == reference_descriptions[i].size() + 1);

        std::vector<char> buf(needed_len);
        render_err = smbios_entry_render(entry, buf.data(), buf.size(), &needed_len);
        assert(render_err == SMBIOS_OK);
        assert(needed_len == reference_descriptions[i].size() + 1);

        // Construct with the explicit returned length, not the C-string
        // constructor: at least one real entry type's render_to_description()
        // can embed a literal NUL byte mid-string (PortConnectionEntry
        // streams a raw uint8_t under operator<<(unsigned char), which
        // inserts the byte as a character when its value happens to be 0 -
        // a pre-existing C++-side quirk, not something smbios_c.cpp's
        // byte-for-byte memcpy introduces). Stopping at the first NUL here
        // would produce a false mismatch against the reference.
        std::string rendered(buf.data(), needed_len - 1);
        if (rendered != reference_descriptions[i]) {
            std::printf("MISMATCH at entry %zu:\n  C API:      %s\n  Reference:  %s\n",
                i, rendered.c_str(), reference_descriptions[i].c_str());
            return 1;
        }
    }

    std::printf("All %zu entries matched the C++ reference byte-for-byte - OK\n", count);

    smbios_close(handle);
    return 0;
}
