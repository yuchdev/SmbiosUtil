#include <libsmbios/output_formatter.hpp>

#include <map>
#include <stdexcept>

using namespace smbios;

namespace {

/// @brief Module-scope registry of output formatters, keyed by
/// format_name(). A function-local static (Meyers-singleton style) rather
/// than a namespace-scope global: static-init order across translation
/// units is unspecified in C++, but a function-local static is guaranteed
/// initialized on first use regardless of which TU's dynamic initializers
/// (i.e. which REGISTER_OUTPUT_FORMATTER invocations) run first. Mirrors
/// smbios_entry_factory.cpp's entry_registry().
std::map<std::string, std::unique_ptr<IOutputFormatter>>& formatter_registry()
{
    static std::map<std::string, std::unique_ptr<IOutputFormatter>> registry;
    return registry;
}

} // namespace

void smbios::register_output_formatter(std::unique_ptr<IOutputFormatter> formatter)
{
    if (!formatter) {
        throw std::invalid_argument("cannot register a null output formatter");
    }
    std::string name = formatter->format_name();
    auto& registry = formatter_registry();
    if (registry.find(name) != registry.end()) {
        // Programming error: two formatter classes (or two
        // REGISTER_OUTPUT_FORMATTER invocations) both claim the same name.
        // Fail fast at static-init time rather than silently overwriting the
        // first registration. The message names only the format name, never
        // any field value (threat model INFO-6).
        throw std::logic_error("output formatter '" + name + "' is already registered");
    }
    registry.emplace(std::move(name), std::move(formatter));
}

const IOutputFormatter* smbios::get_output_formatter(const std::string& name)
{
    auto& registry = formatter_registry();
    auto it = registry.find(name);
    if (it != registry.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<std::string> smbios::registered_output_formatter_names()
{
    // std::map iterates in ascending key order, so the result is sorted.
    const auto& registry = formatter_registry();
    std::vector<std::string> names;
    names.reserve(registry.size());
    for (const auto& item : registry) {
        names.push_back(item.first);
    }
    return names;
}
