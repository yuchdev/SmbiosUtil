#include "support/local_only_guard.hpp"

/// @file process_spawn_test.cpp
/// @brief Local-only regression test of spawn_and_capture()'s descriptor
/// hygiene (milestone 0011, Story 04.0 Task 04; threat model R-16 "no
/// inherited descriptors").
///
/// - Type: integration (local-only tier, labels `gui;gui-local`), POSIX
///   only. Needs no display, GL, clipboard or live table.
/// - Scenario: the test process opens a plain (NOT close-on-exec)
///   descriptor on `/dev/null` and moves it to a number >= 100, then runs
///   `/bin/ls /dev/fd` through spawn_and_capture(); `ls` lists the
///   descriptors open in the child (on Linux `/dev/fd` is a link to
///   `/proc/self/fd`).
/// - Asserts: `ls` exits 0; its listing holds 0, 1 and 2, never the marker
///   descriptor, and at most two other entries: the descriptors `ls` itself
///   opens to read `/dev/fd` (macOS `ls` lists `3` and `4`, its fts
///   directory handles; GNU `ls` one).
/// - Regression: fails against a helper that does not close descriptors
///   above 2 in the child (the marker and every other open descriptor of
///   the test process are listed).
/// - Output on failure: descriptor numbers and counts only (no captured
///   text is printed - T-3).
/// - Skips: as root (R-19). On Windows the case is a no-op pass (the
///   Windows branch limits inheritance through
///   PROC_THREAD_ATTRIBUTE_HANDLE_LIST instead, UNVERIFIED).
/// - On failure, first check: the close loop in process_spawn.cpp's POSIX
///   child branch, and whether `/bin/ls` lists `/dev/fd` on this OS.

#include <boost/test/unit_test.hpp>

#include "support/local_skip.hpp"
#include "support/process_spawn.hpp"

#include <cstddef>
#include <filesystem>
#include <set>
#include <string>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace utf = boost::unit_test;
using smbios::test::gui::skip_if_root;
using smbios::test::gui::spawn_and_capture;
using smbios::test::gui::SpawnResult;
using smbios::test::gui::status_name;

namespace {

constexpr const char* kCaseName = "SpawnDoesNotInheritDescriptors";

#if !defined(_WIN32)
/// Lowest number of the marker descriptor; far above anything ls opens.
constexpr int kMarkerFloor = 100;

/// @brief Owns the marker descriptor for the duration of the case.
class MarkerDescriptor {
public:
    MarkerDescriptor() {
        const int low = ::open("/dev/null", O_RDONLY); // deliberately not O_CLOEXEC
        if (low >= 0) {
            fd_ = ::fcntl(low, F_DUPFD, kMarkerFloor); // F_DUPFD clears close-on-exec
            ::close(low);
        }
    }
    MarkerDescriptor(const MarkerDescriptor&) = delete;
    MarkerDescriptor& operator=(const MarkerDescriptor&) = delete;
    ~MarkerDescriptor() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool inheritable() const noexcept {
        return fd_ >= 0 && (::fcntl(fd_, F_GETFD) & FD_CLOEXEC) == 0;
    }

private:
    int fd_ = -1;
};

/// @brief Parses `ls` output (one name per line) into descriptor numbers;
/// a non-numeric line counts as `-1`.
std::multiset<int> parse_descriptor_listing(const std::string& listing) {
    std::multiset<int> descriptors;
    std::size_t begin = 0;
    while (begin < listing.size()) {
        std::size_t end = listing.find('\n', begin);
        if (end == std::string::npos) {
            end = listing.size();
        }
        int value = 0;
        bool numeric = end > begin && end - begin <= 9;
        for (std::size_t i = begin; numeric && i < end; ++i) {
            const char c = listing[i];
            numeric = c >= '0' && c <= '9';
            value = value * 10 + (c - '0');
        }
        if (end > begin) {
            descriptors.insert(numeric ? value : -1);
        }
        begin = end + 1;
    }
    return descriptors;
}
#endif

} // namespace

BOOST_AUTO_TEST_SUITE(SmbiosGuiLocalTests)

/// Regression: the child inherits only 0, 1 and 2.
BOOST_AUTO_TEST_CASE(SpawnDoesNotInheritDescriptors,
    *utf::precondition([](utf::test_unit_id) { return skip_if_root(kCaseName); }))
{
#if defined(_WIN32)
    BOOST_TEST_MESSAGE(kCaseName << ": POSIX-only check, nothing to do on Windows");
#else
    const MarkerDescriptor marker;
    BOOST_REQUIRE_MESSAGE(marker.inheritable(), "inheritable marker descriptor: opened -> not opened");
    BOOST_REQUIRE_MESSAGE(marker.get() >= kMarkerFloor, "marker descriptor >= 100 -> " << marker.get());

    const SpawnResult result =
        spawn_and_capture("/bin/ls", {"/dev/fd"}, std::filesystem::temp_directory_path());

    BOOST_REQUIRE_MESSAGE((result.status == SpawnResult::Status::Exited),
                          "status: Exited -> " << status_name(result.status));
    BOOST_REQUIRE_MESSAGE((result.exit_code == 0), "exit status: 0 -> " << result.exit_code);

    const std::multiset<int> descriptors = parse_descriptor_listing(result.stdout_bytes);
    for (const int standard : {0, 1, 2}) {
        BOOST_TEST((descriptors.count(standard) == 1),
                   "descriptor " << standard << " listed: 1 -> " << descriptors.count(standard));
    }
    BOOST_TEST((descriptors.count(marker.get()) == 0),
               "inherited marker descriptor " << marker.get() << " listed: 0 -> "
                                              << descriptors.count(marker.get()));
    std::size_t others = 0;
    for (const int fd : descriptors) {
        if (fd < 0 || fd > 2) {
            ++others;
        }
    }
    // The allowed extras are ls's own directory handles (2 on macOS).
    BOOST_TEST((others <= 2), "descriptors other than 0/1/2 listed: <= 2 -> " << others);
#endif
}

BOOST_AUTO_TEST_SUITE_END()
