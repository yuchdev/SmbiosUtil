#include <smbios/smbios_entry_factory.h>
#include <smbios/abstract_smbios_entry.h>
#include <smbios/smbios.h>

#include <map>
#include <sstream>
#include <stdexcept>

using namespace smbios;

namespace {

/// @brief Module-scope registry of entry creators, keyed by SMBiosHandler
/// value. A function-local static (Meyers-singleton style) rather than a
/// namespace-scope global: static-init order across translation units is
/// unspecified in C++, but a function-local static is guaranteed
/// initialized on first use regardless of which TU's dynamic initializers
/// (i.e. which REGISTER_SMBIOS_ENTRY invocations) run first.
std::map<uint8_t, EntryCreator>& entry_registry()
{
    static std::map<uint8_t, EntryCreator> registry;
    return registry;
}

} // namespace

void smbios::register_smbios_entry(uint8_t handler_value, EntryCreator creator)
{
    auto& registry = entry_registry();
    if (registry.find(handler_value) != registry.end()) {
        // Programming error: two entry classes (or two REGISTER_SMBIOS_ENTRY
        // invocations) both claim the same SMBiosHandler value. Fail fast at
        // static-init time rather than silently overwriting the first
        // registration via operator[].
        std::stringstream err;
        err << "SMBIOS entry handler value " << static_cast<unsigned>(handler_value)
            << " is already registered";
        throw std::logic_error(err.str());
    }
    registry.emplace(handler_value, std::move(creator));
}

std::unique_ptr<AbstractSMBiosEntry> smbios::SMBiosEntryFactory::create(const DMIHeader& header,
    const SMBiosVersion& version, const uint8_t* table_end)
{
    auto& registry = entry_registry();
    auto it = registry.find(header.type);
    if (it != registry.end()) {
        return it->second(header, version, table_end);
    }
    // no such index, just proceed
    return nullptr;
}
