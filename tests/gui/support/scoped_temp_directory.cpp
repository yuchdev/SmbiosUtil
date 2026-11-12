#include "support/local_only_guard.hpp"

/// @file scoped_temp_directory.cpp
/// @brief Implementation of ScopedTempDirectory (local-only GUI tier).

#include "support/scoped_temp_directory.hpp"

#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <cstdio>
#include <random>
#else
#include <cstdlib>
#include <unistd.h> // mkdtemp() on macOS
#endif

namespace smbios::test::gui {

namespace {

/// Name prefix of every directory this class creates.
constexpr const char* kPrefix = "smbios_gui_local_wd.";

} // namespace

ScopedTempDirectory::ScopedTempDirectory() {
    std::error_code ec;
    const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
    if (ec || base.empty()) {
        return;
    }
#if !defined(_WIN32)
    std::string pattern = (base / (std::string(kPrefix) + "XXXXXX")).string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    if (::mkdtemp(buffer.data()) == nullptr) {
        return;
    }
    path_ = std::filesystem::path(buffer.data());
#else
    // UNVERIFIED (never compiled on CI): a random name, retried if taken;
    // create_directory() returns false without error when it already exists.
    std::random_device random;
    for (int attempt = 0; attempt < 16; ++attempt) {
        char suffix[17];
        std::snprintf(suffix, sizeof suffix, "%08x%08x", random(), random());
        const std::filesystem::path candidate = base / (std::string(kPrefix) + suffix);
        if (std::filesystem::create_directory(candidate, ec) && !ec) {
            path_ = candidate;
            return;
        }
    }
#endif
}

ScopedTempDirectory::~ScopedTempDirectory() {
    if (valid()) {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
}

std::optional<std::size_t> ScopedTempDirectory::entry_count() const {
    if (!valid()) {
        return std::nullopt;
    }
    std::error_code ec;
    std::size_t count = 0;
    for (std::filesystem::directory_iterator it(path_, ec), end; !ec && it != end; it.increment(ec)) {
        ++count;
    }
    if (ec) {
        return std::nullopt;
    }
    return count;
}

} // namespace smbios::test::gui
