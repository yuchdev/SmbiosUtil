#include <smbios_c/smbios_c.h>

#include <libsmbios/smbios.h>

extern "C" smbios_error_t smbios_open(smbios_handle_t* out_handle)
{
    if (out_handle == nullptr) {
        return SMBIOS_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* smbios = new smbios::SMBios();

        // smbios::SMBios() never throws when every ISmbiosSource in the
        // chain fails - it leaves itself with an empty table instead (see
        // its own doc comment in libsmbios/include/libsmbios/smbios.h).
        // That case has to be detected here, not caught above, since no
        // exception is ever raised for it.
        if (smbios->get_structures_count() == 0) {
            delete smbios;
            *out_handle = nullptr;
            return SMBIOS_ERR_NO_TABLE_FOUND;
        }

        *out_handle = reinterpret_cast<smbios_handle_t>(smbios);
        return SMBIOS_OK;
    } catch (const std::exception&) {
        *out_handle = nullptr;
        return SMBIOS_ERR_UNKNOWN;
    } catch (...) {
        *out_handle = nullptr;
        return SMBIOS_ERR_UNKNOWN;
    }
}

extern "C" void smbios_close(smbios_handle_t handle)
{
    if (handle == nullptr) {
        return;
    }

    try {
        delete reinterpret_cast<smbios::SMBios*>(handle);
    } catch (...) {
        // smbios::SMBios::~SMBios() is currently an empty, non-throwing
        // destructor, but this is a void-returning cleanup function with
        // no error channel to report through, so a defensive catch-all is
        // kept here per the project's coding standard against letting
        // destructors throw across an API boundary.
    }
}
