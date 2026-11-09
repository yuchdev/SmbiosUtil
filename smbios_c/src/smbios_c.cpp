#include <smbios/smbios.h>

#include <libsmbios/smbios.hpp>
#include <libsmbios/abstract_smbios_entry.hpp>
#include <libsmbios/smbios_entry_factory.hpp>
#include <libsmbios/tables/bios_information_entry.hpp>
#include <libsmbios/tables/system_information_entry.hpp>
#include <libsmbios/tables/memory_device_entry.hpp>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

/// @brief Real type behind smbios_entry_handle_t - defined only here, per
/// contract C1, so no caller can see or rely on this layout. Deliberately
/// at file scope, not inside an anonymous namespace: it must be the exact
/// same type the public header forward-declares (`struct
/// smbios_entry_handle_s* smbios_entry_handle_t;`), not an unrelated
/// internally-linked type that merely happens to share a name. Carries
/// both the entry's own DMIHeader and a pointer back to its parent
/// SMBios, since smbios_entry_render() (unlike smbios_entry_count()/
/// smbios_entry_at()) needs the parent's SMBIOS version and table bounds
/// to decode the entry identically to the C++ CLI's own
/// factory.create()/render_to_description() call in smbios/src/main.cpp -
/// a DMIHeader alone cannot supply either.
struct smbios_entry_handle_s
{
    const smbios::DMIHeader* header;
    const smbios::SMBios* parent;
};

/// @brief Real type behind smbios_handle_t - defined only here, per
/// contract C1. Owns the real smbios::SMBios plus every entry handle ever
/// handed out through it via smbios_entry_at(), so entry handles stay
/// valid for exactly as long as documented ("valid until smbios_close()",
/// smbios.h) and are freed together with their parent, never
/// separately - there is deliberately no smbios_entry_close() function.
struct smbios_handle_s
{
    std::unique_ptr<smbios::SMBios> bios;
    std::vector<std::unique_ptr<smbios_entry_handle_s>> entries;
};

extern "C" smbios_error_t smbios_open(smbios_handle_t* out_handle)
{
    if (out_handle == nullptr) {
        return SMBIOS_ERR_INVALID_ARGUMENT;
    }

    try {
        auto bios = std::make_unique<smbios::SMBios>();

        // smbios::SMBios() never throws when every ISmbiosSource in the
        // chain fails - it leaves itself with an empty table instead (see
        // its own doc comment in libsmbios/include/libsmbios/smbios.hpp).
        // That case has to be detected here, not caught above, since no
        // exception is ever raised for it.
        if (bios->get_structures_count() == 0) {
            *out_handle = nullptr;
            return SMBIOS_ERR_NO_TABLE_FOUND;
        }

        auto* impl = new smbios_handle_s{std::move(bios), {}};
        *out_handle = reinterpret_cast<smbios_handle_t>(impl);
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
        delete reinterpret_cast<smbios_handle_s*>(handle);
    } catch (...) {
        // Every member here is a self-managed smart pointer/vector with a
        // non-throwing destructor, but this is a void-returning cleanup
        // function with no error channel to report through, so a
        // defensive catch-all is kept per the project's coding standard
        // against letting destructors throw across an API boundary.
    }
}

namespace {

/// @brief Builds the same human-readable description the CLI prints for
/// one structure (smbios/src/main.cpp's own factory.create() +
/// render_to_description() call), with the one addition this C API needs:
/// a placeholder string instead of silently producing nothing when the
/// factory returns nullptr for an unregistered type.
std::string render_entry_description(const smbios::SMBios& bios, const smbios::DMIHeader& header)
{
    smbios::SMBiosEntryFactory factory;
    const uint8_t* table_end = bios.get_table_base() + bios.get_table_size();
    std::unique_ptr<smbios::AbstractSMBiosEntry> entry = factory.create(header, bios.get_smbios_version(), table_end);

    if (!entry) {
        return "Unknown structure type " + std::to_string(static_cast<unsigned>(header.type));
    }

    return entry->render_to_description();
}

} // namespace

extern "C" smbios_error_t smbios_entry_count(smbios_handle_t handle, size_t* out_count)
{
    if (handle == nullptr || out_count == nullptr) {
        return SMBIOS_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* impl = reinterpret_cast<smbios_handle_s*>(handle);

        // SMBios::iterator defines none of the standard iterator_traits
        // typedefs (no difference_type), so std::distance() cannot be
        // used here - count by hand instead.
        size_t count = 0;
        for (auto it = impl->bios->begin(), end = impl->bios->end(); it != end; ++it) {
            ++count;
        }

        *out_count = count;
        return SMBIOS_OK;
    } catch (...) {
        return SMBIOS_ERR_UNKNOWN;
    }
}

extern "C" smbios_error_t smbios_entry_at(smbios_handle_t handle, size_t index, smbios_entry_handle_t* out_entry)
{
    if (handle == nullptr || out_entry == nullptr) {
        return SMBIOS_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* impl = reinterpret_cast<smbios_handle_s*>(handle);

        // SMBios::iterator is bidirectional, not random-access, so index
        // lookup is a bounds-checked linear walk - std::advance() would
        // blindly increment past end() on an out-of-range index, which is
        // undefined behavior for a std::vector iterator.
        auto it = impl->bios->begin();
        const auto end = impl->bios->end();
        for (size_t i = 0; i < index && it != end; ++i, ++it) {
        }

        if (it == end) {
            *out_entry = nullptr;
            return SMBIOS_ERR_INVALID_ARGUMENT;
        }

        // *it returns const DMIHeader& into SMBios's own, never-mutated-
        // after-construction header list - this reference's address is
        // stable for the parent handle's lifetime. The wrapper itself
        // (smbios_entry_handle_s) is owned by impl->entries, so it lives
        // exactly as long as impl does and needs no separate free.
        auto entry_handle = std::make_unique<smbios_entry_handle_s>();
        entry_handle->header = &(*it);
        entry_handle->parent = impl->bios.get();

        smbios_entry_handle_t raw = reinterpret_cast<smbios_entry_handle_t>(entry_handle.get());
        impl->entries.push_back(std::move(entry_handle));

        *out_entry = raw;
        return SMBIOS_OK;
    } catch (...) {
        *out_entry = nullptr;
        return SMBIOS_ERR_UNKNOWN;
    }
}

extern "C" uint8_t smbios_entry_type(smbios_entry_handle_t entry)
{
    if (entry == nullptr) {
        return 0;
    }
    return reinterpret_cast<const smbios_entry_handle_s*>(entry)->header->type;
}

extern "C" uint8_t smbios_entry_length(smbios_entry_handle_t entry)
{
    if (entry == nullptr) {
        return 0;
    }
    return reinterpret_cast<const smbios_entry_handle_s*>(entry)->header->length;
}

extern "C" uint16_t smbios_entry_handle_id(smbios_entry_handle_t entry)
{
    if (entry == nullptr) {
        return 0;
    }
    return reinterpret_cast<const smbios_entry_handle_s*>(entry)->header->handle;
}

extern "C" smbios_error_t smbios_entry_render(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len)
{
    if (entry == nullptr || out_needed_len == nullptr) {
        return SMBIOS_ERR_INVALID_ARGUMENT;
    }

    try {
        const auto* wrapper = reinterpret_cast<const smbios_entry_handle_s*>(entry);
        std::string description = render_entry_description(*wrapper->parent, *wrapper->header);

        const size_t needed = description.size() + 1; // + null terminator
        *out_needed_len = needed;

        if (buf == nullptr || buf_len < needed) {
            return SMBIOS_ERR_BUFFER_TOO_SMALL;
        }

        std::memcpy(buf, description.c_str(), needed);
        return SMBIOS_OK;
    } catch (...) {
        return SMBIOS_ERR_UNKNOWN;
    }
}

namespace {

/// @brief Shared implementation behind every curated typed getter
/// (smbios_bios_vendor(), etc.): constructs the concrete entry the same
/// way smbios_entry_render() does, dynamic_casts it to the type the
/// caller asked for, and - on success - applies the same
/// caller-provided-buffer pattern already used by smbios_entry_render().
/// A failed dynamic_cast (wrong structure type for this getter) reports
/// SMBIOS_ERR_INVALID_ARGUMENT, per this task's own constraint that a
/// caller must check smbios_entry_type() first if unsure.
template <typename Concrete>
smbios_error_t typed_string_getter(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len,
    std::string (Concrete::*getter)() const)
{
    if (entry == nullptr || out_needed_len == nullptr) {
        return SMBIOS_ERR_INVALID_ARGUMENT;
    }

    try {
        const auto* wrapper = reinterpret_cast<const smbios_entry_handle_s*>(entry);

        smbios::SMBiosEntryFactory factory;
        const uint8_t* table_end = wrapper->parent->get_table_base() + wrapper->parent->get_table_size();
        std::unique_ptr<smbios::AbstractSMBiosEntry> concrete_entry =
            factory.create(*wrapper->header, wrapper->parent->get_smbios_version(), table_end);

        auto* typed = dynamic_cast<Concrete*>(concrete_entry.get());
        if (typed == nullptr) {
            return SMBIOS_ERR_INVALID_ARGUMENT;
        }

        std::string value = (typed->*getter)();
        const size_t needed = value.size() + 1; // + null terminator
        *out_needed_len = needed;

        if (buf == nullptr || buf_len < needed) {
            return SMBIOS_ERR_BUFFER_TOO_SMALL;
        }

        std::memcpy(buf, value.c_str(), needed);
        return SMBIOS_OK;
    } catch (...) {
        return SMBIOS_ERR_UNKNOWN;
    }
}

} // namespace

extern "C" smbios_error_t smbios_bios_vendor(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len)
{
    return typed_string_getter<smbios::BiosInformationEntry>(entry, buf, buf_len, out_needed_len, &smbios::BiosInformationEntry::get_vendor_string);
}

extern "C" smbios_error_t smbios_bios_version(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len)
{
    return typed_string_getter<smbios::BiosInformationEntry>(entry, buf, buf_len, out_needed_len, &smbios::BiosInformationEntry::get_version_string);
}

extern "C" smbios_error_t smbios_system_uuid(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len)
{
    return typed_string_getter<smbios::SystemInformationEntry>(entry, buf, buf_len, out_needed_len, &smbios::SystemInformationEntry::get_uuid_string);
}

extern "C" smbios_error_t smbios_memory_device_size(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len)
{
    return typed_string_getter<smbios::MemoryDeviceEntry>(entry, buf, buf_len, out_needed_len, &smbios::MemoryDeviceEntry::get_device_size_string);
}

extern "C" smbios_error_t smbios_memory_device_speed(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len)
{
    return typed_string_getter<smbios::MemoryDeviceEntry>(entry, buf, buf_len, out_needed_len, &smbios::MemoryDeviceEntry::get_device_speed_string);
}
