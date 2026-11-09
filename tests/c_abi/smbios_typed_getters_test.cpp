// Exercises the 5 curated typed field accessors against the real
// machine's SMBIOS table: each on its correct entry type (output must
// match the equivalent C++ getter exactly), and each on a mismatched
// entry type (must return SMBIOS_ERR_INVALID_ARGUMENT). Not wired into
// CMake yet (Story 03.0's job); compiled and run manually.
#include <smbios/smbios.h>

#include <libsmbios/smbios.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/tables/bios_information_entry.hpp>
#include <libsmbios/tables/system_information_entry.hpp>
#include <libsmbios/tables/memory_device_entry.hpp>

#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

/// @brief Calls a smbios_c getter with the "ask needed size, then fetch"
/// two-call idiom and returns the resulting string. Asserts success.
std::string fetch(smbios_error_t (*getter)(smbios_entry_handle_t, char*, size_t, size_t*), smbios_entry_handle_t entry)
{
    size_t needed = 0;
    smbios_error_t err = getter(entry, nullptr, 0, &needed);
    assert(err == SMBIOS_ERR_BUFFER_TOO_SMALL);

    std::vector<char> buf(needed);
    err = getter(entry, buf.data(), buf.size(), &needed);
    assert(err == SMBIOS_OK);
    return std::string(buf.data(), needed - 1); // explicit length, not the C-string ctor - see smbios_entry_iteration_test.cpp's comment on embedded NULs
}

} // namespace

int main()
{
    smbios_handle_t handle = nullptr;
    smbios_error_t err = smbios_open(&handle);
    if (err != SMBIOS_OK) {
        std::printf("smbios_open() returned error %d - no table on this host, skipping typed-getter checks\n",
            static_cast<int>(err));
        return 0;
    }

    smbios::SMBios reference_bios;
    smbios::SMBiosEntryFactory factory;
    const uint8_t* table_end = reference_bios.get_table_base() + reference_bios.get_table_size();

    size_t count = 0;
    assert(smbios_entry_count(handle, &count) == SMBIOS_OK);

    bool saw_bios_info = false;
    bool saw_system_info = false;
    bool saw_memory_device = false;
    bool saw_mismatch_check = false;

    size_t reference_index = 0;
    for (const smbios::DMIHeader& header : reference_bios) {
        smbios_entry_handle_t entry = nullptr;
        assert(smbios_entry_at(handle, reference_index, &entry) == SMBIOS_OK);
        ++reference_index;

        std::unique_ptr<smbios::AbstractSMBiosEntry> concrete =
            factory.create(header, reference_bios.get_smbios_version(), table_end);

        if (auto* bios_info = dynamic_cast<smbios::BiosInformationEntry*>(concrete.get())) {
            saw_bios_info = true;
            std::string vendor = fetch(smbios_bios_vendor, entry);
            std::string version = fetch(smbios_bios_version, entry);
            if (vendor != bios_info->get_vendor_string() || version != bios_info->get_version_string()) {
                std::printf("MISMATCH: BIOS vendor/version via C API vs C++\n");
                return 1;
            }

            // Mismatched-type checks: a BIOS Information entry is not a
            // System Information or Memory Device entry.
            size_t needed = 0;
            assert(smbios_system_uuid(entry, nullptr, 0, &needed) == SMBIOS_ERR_INVALID_ARGUMENT);
            assert(smbios_memory_device_size(entry, nullptr, 0, &needed) == SMBIOS_ERR_INVALID_ARGUMENT);
            saw_mismatch_check = true;
        } else if (auto* system_info = dynamic_cast<smbios::SystemInformationEntry*>(concrete.get())) {
            saw_system_info = true;
            std::string uuid = fetch(smbios_system_uuid, entry);
            if (uuid != system_info->get_uuid_string()) {
                std::printf("MISMATCH: System UUID via C API vs C++\n");
                return 1;
            }

            size_t needed = 0;
            assert(smbios_bios_vendor(entry, nullptr, 0, &needed) == SMBIOS_ERR_INVALID_ARGUMENT);
        } else if (auto* memory_device = dynamic_cast<smbios::MemoryDeviceEntry*>(concrete.get())) {
            saw_memory_device = true;
            std::string size_str = fetch(smbios_memory_device_size, entry);
            std::string speed_str = fetch(smbios_memory_device_speed, entry);
            if (size_str != memory_device->get_device_size_string() || speed_str != memory_device->get_device_speed_string()) {
                std::printf("MISMATCH: Memory Device size/speed via C API vs C++\n");
                return 1;
            }

            size_t needed = 0;
            assert(smbios_bios_vendor(entry, nullptr, 0, &needed) == SMBIOS_ERR_INVALID_ARGUMENT);
        }
    }

    std::printf("BIOSInformation seen: %s, SystemInformation seen: %s, MemoryDevice seen: %s\n",
        saw_bios_info ? "yes" : "no", saw_system_info ? "yes" : "no", saw_memory_device ? "yes" : "no");
    std::printf("All curated getters matched their C++ equivalents, mismatched-type checks %s\n",
        saw_mismatch_check ? "passed" : "not exercised (no BIOSInformation entry on this host)");

    smbios_close(handle);
    return 0;
}
