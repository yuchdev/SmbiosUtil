#pragma once
#include <cstdint>
#include <functional>
#include <memory>

#include <libsmbios/registration_name.hpp>

namespace smbios {

class AbstractSMBiosEntry;
struct DMIHeader;
struct SMBiosVersion;

/// @brief Signature of a concrete SMBIOS entry's creator function, as
/// registered against a SMBios::SMBiosHandler value by REGISTER_SMBIOS_ENTRY.
/// @param table_end one-past-the-end pointer of the mapped SMBIOS structure
/// table; forwarded unchanged to the concrete entry's constructor so its
/// string-section parsing never reads past the buffer.
using EntryCreator = std::function<std::unique_ptr<AbstractSMBiosEntry>(const DMIHeader&, const SMBiosVersion&, const uint8_t*)>;

/// @brief Registers `creator` against `handler_value` in the module-scope
/// entry registry. Intended to be called once at static-init time, via the
/// REGISTER_SMBIOS_ENTRY macro, from each concrete entry's own translation
/// unit - never call this directly from production code outside that macro.
/// @param handler_value a SMBios::SMBiosHandler value, as uint8_t.
/// @param creator factory function producing a concrete AbstractSMBiosEntry.
/// @throws std::logic_error if `handler_value` is already registered; a
/// duplicate registration is a programming error (e.g. two entry classes
/// both claiming the same structure type), never a silent overwrite.
void register_smbios_entry(uint8_t handler_value, EntryCreator creator);

/// @brief Abstract factory class. Generates SMBIOS entry instances
/// according to a header's type and SMBIOS version, via the module-scope
/// self-registration registry populated at static-init time by
/// REGISTER_SMBIOS_ENTRY invocations in each concrete entry's own
/// translation unit. Stateless: holds no per-instance data, since the
/// registry itself lives at module scope (see smbios_entry_factory.cpp).
class SMBiosEntryFactory {
public:

    /// @brief Create concrete instance of the SMBIOS entry via the
    /// self-registered creator for header.type.
    /// @param table_end one-past-the-end pointer of the mapped SMBIOS
    /// structure table; forwarded to the concrete entry's constructor so its
    /// string-section parsing never reads past the buffer.
    /// @return a new entry instance, or nullptr if header.type has no
    /// registered creator.
    std::unique_ptr<AbstractSMBiosEntry> create(const DMIHeader&, const SMBiosVersion&, const uint8_t* table_end);
};

} // namespace smbios

/// @brief Registers `class_name` against SMBiosHandler value `handler_value`
/// at static-init time, via a function-local static map (see
/// smbios_entry_factory.cpp) - no edit to smbios_entry_factory.{h,cpp} is
/// required to add a new entry type. `class_name` must be constructible as
/// `class_name(const smbios::DMIHeader&, const smbios::SMBiosVersion&, const uint8_t*)`.
/// Place exactly one invocation at namespace scope in the entry class's own
/// .cpp file (not its header - a header-placed invocation would register
/// once per translation unit that includes it, tripping the duplicate-
/// registration guard the first time two TUs in the same binary both
/// `#include` that header). `class_name` may be namespace-qualified (e.g.
/// `vendor::OemEntry`): the registration variable's name comes from
/// SMBIOS_DETAIL_UNIQUE_NAME, not from pasting `class_name`.
#define REGISTER_SMBIOS_ENTRY(handler_value, class_name)                                   \
    namespace {                                                                            \
    const bool SMBIOS_DETAIL_UNIQUE_NAME(smbios_entry_registered_) = [] {                  \
        smbios::register_smbios_entry(static_cast<uint8_t>(handler_value),                 \
            [](const smbios::DMIHeader& h, const smbios::SMBiosVersion& v,                 \
               const uint8_t* table_end) {                                                 \
                return std::make_unique<class_name>(h, v, table_end);                       \
            });                                                                            \
        return true;                                                                       \
    }();                                                                                   \
    }
