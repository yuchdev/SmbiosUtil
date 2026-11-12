#include "support/local_only_guard.hpp"

/// @file live_table_support.cpp
/// @brief Implementation of the local-only tier's live-table helpers.

#include "support/live_table_support.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <new>

#include <smbios/gui/copy_as.hpp>

#if !defined(SMBIOS_CLI_PATH)
#error "SMBIOS_CLI_PATH must name the smbios CLI binary; see tests/gui/CMakeLists.txt"
#endif

namespace smbios::test::gui {

namespace {

/// Highest SMBIOS structure type code (types are one byte).
constexpr std::size_t kMaxTypeCode = 255;

/// @return true iff `haystack` contains `needle`, ASCII case-insensitively.
bool contains_ignoring_case(std::string_view haystack, std::string_view needle) {
    if (needle.size() > haystack.size()) {
        return false;
    }
    for (std::size_t start = 0; start + needle.size() <= haystack.size(); ++start) {
        bool match = true;
        for (std::size_t i = 0; i < needle.size(); ++i) {
            const auto a = static_cast<unsigned char>(haystack[start + i]);
            const auto b = static_cast<unsigned char>(needle[i]);
            if (std::tolower(a) != std::tolower(b)) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

/// @return true iff no field name of `node` names a device identifier.
bool has_no_identifier_field_names(const smbios::gui::EntryNode& node) {
    for (const smbios::gui::FieldRow& row : node.fields) {
        if (contains_ignoring_case(row.name, "serial") || contains_ignoring_case(row.name, "uuid")
            || contains_ignoring_case(row.name, "asset")) {
            return false;
        }
    }
    return true;
}

/// @return true iff `c` is JSON insignificant whitespace (or any ASCII
/// whitespace, which the formatters never emit outside strings).
bool is_ascii_space(char c) noexcept {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\v';
}

/// Types threat model T-2 lists as carrying serials/UUIDs/asset tags.
bool is_identifier_type(std::size_t type) noexcept {
    return type == 1 || type == 2 || type == 3 || type == 4 || type == 11 || type == 17;
}

} // namespace

std::string hex_handle(std::uint16_t handle) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << handle;
    return out.str();
}

bool same_bytes(std::string_view expected, std::string_view actual) noexcept {
    return expected.size() == actual.size() && expected == actual;
}

std::optional<std::size_t> find_bytes(std::string_view haystack, std::string_view needle,
                                      std::size_t from) noexcept {
    if (from > haystack.size()) {
        return std::nullopt;
    }
    const std::size_t position = haystack.find(needle, from);
    if (position == std::string_view::npos) {
        return std::nullopt;
    }
    return position;
}

std::vector<std::size_t> LiveGuiModel::registered_indices() const {
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < kinds.size(); ++i) {
        if (kinds[i] != NodeKind::Unregistered) {
            indices.push_back(i);
        }
    }
    return indices;
}

std::size_t LiveGuiModel::unregistered_count() const {
    std::size_t count = 0;
    for (const NodeKind kind : kinds) {
        count += kind == NodeKind::Unregistered ? 1 : 0;
    }
    return count;
}

LiveGuiModel read_live_gui_model() {
    LiveGuiModel model;
    try {
        model.bios = std::make_unique<smbios::SMBios>();
        model.nodes = smbios::gui::build_entry_tree(*model.bios);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (...) {
        // The exception's text is never kept or printed (threat model T-3).
        model.nodes.clear();
        model.read_failed = true;
        return model;
    }
    model.kinds.reserve(model.nodes.size());
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        if (model.nodes[i].decode_failed) {
            model.kinds.push_back(NodeKind::DecodeFailed);
            continue;
        }
        try {
            const auto entry = smbios::gui::create_entry_for_node(*model.bios, i, model.nodes[i]);
            model.kinds.push_back(entry ? NodeKind::Registered : NodeKind::Unregistered);
        } catch (const std::bad_alloc&) {
            throw;
        } catch (...) {
            model.kinds.push_back(NodeKind::DecodeFailed);
        }
    }
    return model;
}

std::unique_ptr<smbios::AbstractSMBiosEntry> entry_for_node(LiveGuiModel& model, std::size_t node_index) {
    if (!model.bios || node_index >= model.nodes.size()) {
        return nullptr;
    }
    try {
        return smbios::gui::create_entry_for_node(*model.bios, node_index, model.nodes[node_index]);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (...) {
        return nullptr;
    }
}

SpawnResult run_cli(const GuiCopyFormat& format, const std::filesystem::path& working_directory) {
    return spawn_and_capture(SMBIOS_CLI_PATH, {"--format", format.cli_argument}, working_directory);
}

std::string_view trim_ascii_whitespace(std::string_view s) noexcept {
    while (!s.empty() && is_ascii_space(s.front())) {
        s.remove_prefix(1);
    }
    while (!s.empty() && is_ascii_space(s.back())) {
        s.remove_suffix(1);
    }
    return s;
}

std::optional<std::vector<std::string_view>> split_json_array(std::string_view document) {
    std::size_t i = 0;
    while (i < document.size() && is_ascii_space(document[i])) {
        ++i;
    }
    if (i >= document.size() || document[i] != '[') {
        return std::nullopt;
    }
    ++i;
    std::vector<std::string_view> elements;
    std::size_t element_start = i;
    std::size_t depth = 0;
    bool in_string = false;
    for (; i < document.size(); ++i) {
        const char c = document[i];
        if (in_string) {
            if (c == '\\') {
                ++i; // skip the escaped character
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '{' || c == '[') {
            ++depth;
        } else if (c == '}' || c == ']') {
            if (depth > 0) {
                --depth;
                continue;
            }
            if (c != ']') {
                return std::nullopt;
            }
            const std::string_view last = trim_ascii_whitespace(document.substr(element_start, i - element_start));
            if (!last.empty()) {
                elements.push_back(last);
            } else if (!elements.empty()) {
                return std::nullopt; // trailing comma
            }
            if (!trim_ascii_whitespace(document.substr(i + 1)).empty()) {
                return std::nullopt; // bytes after the array
            }
            return elements;
        } else if (c == ',' && depth == 0) {
            const std::string_view element = trim_ascii_whitespace(document.substr(element_start, i - element_start));
            if (element.empty()) {
                return std::nullopt;
            }
            elements.push_back(element);
            element_start = i + 1;
        }
    }
    return std::nullopt; // unterminated array or string
}

std::optional<std::string> json_entry_type_name(std::string_view element) {
    constexpr std::string_view kTypeKey = "\"type\"";
    std::string_view rest = trim_ascii_whitespace(element);
    if (rest.empty() || rest.front() != '{') {
        return std::nullopt;
    }
    rest = trim_ascii_whitespace(rest.substr(1));
    if (rest.substr(0, kTypeKey.size()) != kTypeKey) {
        return std::nullopt;
    }
    rest = trim_ascii_whitespace(rest.substr(kTypeKey.size()));
    if (rest.empty() || rest.front() != ':') {
        return std::nullopt;
    }
    rest = trim_ascii_whitespace(rest.substr(1));
    if (rest.empty() || rest.front() != '"') {
        return std::nullopt;
    }
    std::string name;
    for (std::size_t i = 1; i < rest.size(); ++i) {
        const char c = rest[i];
        if (c == '"') {
            return name;
        }
        if (c == '\\') {
            if (i + 1 >= rest.size()) {
                return std::nullopt;
            }
            const char escaped = rest[++i];
            // Type names are fixed ASCII; only the simple escapes can occur.
            if (escaped != '"' && escaped != '\\' && escaped != '/') {
                return std::nullopt;
            }
            name.push_back(escaped);
            continue;
        }
        name.push_back(c);
    }
    return std::nullopt;
}

std::optional<std::size_t> type_code_for_name(std::string_view name) {
    for (std::size_t type = 0; type <= kMaxTypeCode; ++type) {
        const std::string label = smbios::gui::entry_type_label(type);
        if (label.rfind("Unknown (", 0) == 0) {
            continue;
        }
        if (label == name) {
            return type;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> find_identifier_free_node(const LiveGuiModel& model) {
    const auto qualifies = [&model](std::size_t i) {
        return model.kinds[i] == NodeKind::Registered && !model.nodes[i].fields.empty()
            && has_no_identifier_field_names(model.nodes[i]);
    };
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        if (model.nodes[i].type == 0 && qualifies(i)) {
            return i;
        }
    }
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        if (!is_identifier_type(model.nodes[i].type) && qualifies(i)) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace smbios::test::gui
