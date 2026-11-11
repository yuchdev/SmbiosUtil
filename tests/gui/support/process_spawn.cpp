#include "support/local_only_guard.hpp"

/// @file process_spawn.cpp
/// @brief Implementation of spawn_and_capture() (local-only GUI tier; see
/// process_spawn.hpp for the rules). POSIX: fork + execv, poll()-drained
/// pipes, deadline kill of the child's process group. Windows (UNVERIFIED,
/// never compiled on CI): CreateProcessW + Job Object + reader threads.

#include "support/process_spawn.hpp"

#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <functional>
#include <thread>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace smbios::test::gui {

std::string_view status_name(SpawnResult::Status status) noexcept {
    switch (status) {
    case SpawnResult::Status::Exited: return "Exited";
    case SpawnResult::Status::Signaled: return "Signaled";
    case SpawnResult::Status::TimedOut: return "TimedOut";
    case SpawnResult::Status::OutputLimitExceeded: return "OutputLimitExceeded";
    case SpawnResult::Status::SpawnFailed: return "SpawnFailed";
    }
    return "Unknown";
}

namespace {

using Clock = std::chrono::steady_clock;

/// @brief Checks the path preconditions shared by both platforms.
/// @return true if `executable` is an absolute path to a regular file and
///         `working_directory` an absolute path to a directory.
bool paths_are_valid(const std::filesystem::path& executable,
                     const std::filesystem::path& working_directory) {
    std::error_code ec;
    if (!executable.is_absolute() || !std::filesystem::is_regular_file(executable, ec) || ec) {
        return false;
    }
    if (!working_directory.is_absolute()
        || !std::filesystem::is_directory(working_directory, ec) || ec) {
        return false;
    }
    return true;
}

} // namespace

#if !defined(_WIN32)

namespace {

/// @brief Owns one POSIX file descriptor; closes it exactly once.
class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) noexcept : fd_(fd) {}
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept : fd_(other.release()) {}
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }
    ~UniqueFd() { reset(); }

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
    int release() noexcept {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }
    void reset(int fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_ = -1;
};

/// @brief Creates a pipe whose both ends are close-on-exec.
/// @return true on success; `read_end`/`write_end` own the descriptors.
bool make_cloexec_pipe(UniqueFd& read_end, UniqueFd& write_end) {
    int fds[2] = {-1, -1};
#if defined(__linux__)
    // Atomic: no window in which another thread's fork() could inherit them.
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        return false;
    }
    read_end.reset(fds[0]);
    write_end.reset(fds[1]);
    return true;
#else
    // pipe() + fcntl() is not atomic, but the local tier's test process is
    // single-threaded and no other code forks while this runs; the child
    // also closes every descriptor above 2 itself (see spawn_and_capture()).
    if (::pipe(fds) != 0) {
        return false;
    }
    read_end.reset(fds[0]);
    write_end.reset(fds[1]);
    return ::fcntl(fds[0], F_SETFD, FD_CLOEXEC) == 0 && ::fcntl(fds[1], F_SETFD, FD_CLOEXEC) == 0;
#endif
}

/// Upper bound (exclusive) on the descriptors the child closes before
/// execv() when the descriptor limit is unknown or larger.
constexpr long kMaxDescriptorsToClose = 65536;

/// @brief Exclusive upper bound on open descriptor numbers, computed in the
/// parent before fork() (sysconf() is not async-signal-safe).
/// @return `sysconf(_SC_OPEN_MAX)` capped to [3, kMaxDescriptorsToClose];
///         kMaxDescriptorsToClose if the limit is unknown.
int descriptor_close_limit() noexcept {
    const long limit = ::sysconf(_SC_OPEN_MAX);
    if (limit <= 0 || limit > kMaxDescriptorsToClose) {
        return static_cast<int>(kMaxDescriptorsToClose);
    }
    return static_cast<int>(limit < 3 ? 3 : limit);
}

/// @brief waitpid() retried on EINTR.
pid_t wait_for(pid_t pid, int& wait_status, int options) {
    pid_t result = -1;
    do {
        result = ::waitpid(pid, &wait_status, options);
    } while (result < 0 && errno == EINTR);
    return result;
}

/// @brief Kills the child's process group and the child, then reaps it.
void kill_and_reap(pid_t pid, int& wait_status) {
    ::kill(-pid, SIGKILL); // the child called setpgid(0, 0)
    ::kill(pid, SIGKILL);  // in case setpgid() had not run yet
    wait_for(pid, wait_status, 0);
}

} // namespace

SpawnResult spawn_and_capture(LiteralArg executable, std::span<const LiteralArg> arguments,
                              const std::filesystem::path& working_directory) {
    SpawnResult result;
    const std::filesystem::path executable_path(executable.c_str());
    if (!paths_are_valid(executable_path, working_directory)) {
        return result;
    }

    // Everything the child needs is prepared before fork(): after fork() in
    // a possibly multi-threaded process the child may only make
    // async-signal-safe calls (setpgid, dup2, chdir, close, signal, execv,
    // write, _exit).
    // execv() takes char* const[] but does not modify the strings; the
    // literals live in static storage.
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 2);
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (const LiteralArg& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    const std::string working_directory_text = working_directory.string();

    UniqueFd stdout_read;
    UniqueFd stdout_write;
    UniqueFd stderr_read;
    UniqueFd stderr_write;
    // Written by the child only if chdir()/execv() fails; a successful
    // execv() closes it (close-on-exec), so the parent then reads EOF.
    UniqueFd exec_error_read;
    UniqueFd exec_error_write;
    if (!make_cloexec_pipe(stdout_read, stdout_write) || !make_cloexec_pipe(stderr_read, stderr_write)
        || !make_cloexec_pipe(exec_error_read, exec_error_write)) {
        return result;
    }
    UniqueFd null_input(::open("/dev/null", O_RDONLY | O_CLOEXEC));
    if (!null_input.valid()) {
        return result;
    }

    const int close_limit = descriptor_close_limit();
    const Clock::time_point start = Clock::now();
    const Clock::time_point deadline = start + kSpawnDeadline;
    const pid_t pid = ::fork();
    if (pid < 0) {
        return result;
    }
    if (pid == 0) {
        // Child. Own process group, so a deadline kill also reaches anything
        // it starts. dup2() clears close-on-exec on the target descriptors.
        ::setpgid(0, 0);
        if (::dup2(null_input.get(), STDIN_FILENO) < 0 || ::dup2(stdout_write.get(), STDOUT_FILENO) < 0
            || ::dup2(stderr_write.get(), STDERR_FILENO) < 0
            || ::chdir(working_directory_text.c_str()) != 0) {
            const int error = errno;
            (void)!::write(exec_error_write.get(), &error, sizeof error);
            ::_exit(127);
        }
        // Inherit nothing but 0, 1 and 2 (R-16): close every other
        // descriptor the test process had open (GLFW/window-system
        // connections, Boost.Test's files, ...), whether or not it is
        // close-on-exec. The exec-error pipe's write end stays open until
        // execv() (it is close-on-exec) so the parent still learns of an
        // exec failure. close() and signal() are async-signal-safe;
        // close_limit was computed before fork().
        const int exec_error_fd = exec_error_write.get();
        for (int fd = 3; fd < close_limit; ++fd) {
            if (fd != exec_error_fd) {
                ::close(fd);
            }
        }
        // execv() keeps an ignored disposition: give the child the default
        // SIGPIPE even if the test process ignores it.
        ::signal(SIGPIPE, SIG_DFL);
        ::execv(argv[0], argv.data());
        const int error = errno;
        (void)!::write(exec_error_write.get(), &error, sizeof error);
        ::_exit(127);
    }

    // Parent: drop the child's ends so EOF arrives when the child exits.
    stdout_write.reset();
    stderr_write.reset();
    exec_error_write.reset();
    null_input.reset();

    int wait_status = 0;
    int exec_error = 0;
    ssize_t exec_error_bytes = 0;
    do {
        exec_error_bytes = ::read(exec_error_read.get(), &exec_error, sizeof exec_error);
    } while (exec_error_bytes < 0 && errno == EINTR);
    if (exec_error_bytes < 0) {
        // The read itself failed: the child's state is unknown and it may
        // be running, so never wait on it unbounded - kill it (and its
        // process group), reap it and report SpawnFailed.
        kill_and_reap(pid, wait_status);
        return result;
    }
    if (exec_error_bytes > 0) {
        // chdir()/execv() failed in the child: it is exiting with 127;
        // reap it and report SpawnFailed.
        wait_for(pid, wait_status, 0);
        return result;
    }

    // Drain stdout and stderr together until both reach EOF, the output cap
    // is exceeded or the deadline passes.
    bool timed_out = false;
    bool over_cap = false;
    std::size_t total_bytes = 0;
    struct Stream {
        UniqueFd* fd;
        std::string* sink;
    };
    Stream streams[2] = {{&stdout_read, &result.stdout_bytes}, {&stderr_read, &result.stderr_bytes}};
    char buffer[65536];
    while ((stdout_read.valid() || stderr_read.valid()) && !over_cap) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
        if (remaining.count() <= 0) {
            timed_out = true;
            break;
        }
        pollfd fds[2];
        nfds_t count = 0;
        Stream* polled[2] = {nullptr, nullptr};
        for (Stream& stream : streams) {
            if (stream.fd->valid()) {
                fds[count] = pollfd{stream.fd->get(), POLLIN, 0};
                polled[count] = &stream;
                ++count;
            }
        }
        const int ready = ::poll(fds, count, static_cast<int>(remaining.count()));
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            timed_out = true; // cannot drain any more: treat like a deadline
            break;
        }
        for (nfds_t i = 0; i < count; ++i) {
            if ((fds[i].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) == 0) {
                continue;
            }
            const ssize_t n = ::read(fds[i].fd, buffer, sizeof buffer);
            if (n > 0) {
                const auto bytes = static_cast<std::size_t>(n);
                if (total_bytes + bytes > kSpawnOutputCap) {
                    over_cap = true;
                    break;
                }
                total_bytes += bytes;
                polled[i]->sink->append(buffer, bytes);
            } else if (n == 0 || errno != EINTR) {
                polled[i]->fd->reset(); // EOF or a read error: stop polling it
            }
        }
    }

    if (!timed_out && !over_cap) {
        // Both pipes closed; wait for the exit itself, still under the
        // deadline (a child could close its outputs and keep running).
        for (;;) {
            const pid_t reaped = wait_for(pid, wait_status, WNOHANG);
            if (reaped == pid) {
                break;
            }
            if (reaped < 0) {
                result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
                return result; // cannot reap: SpawnFailed
            }
            if (Clock::now() >= deadline) {
                timed_out = true;
                break;
            }
            ::poll(nullptr, 0, 10);
        }
    }
    if (timed_out || over_cap) {
        kill_and_reap(pid, wait_status);
        result.status = timed_out ? SpawnResult::Status::TimedOut
                                  : SpawnResult::Status::OutputLimitExceeded;
    } else if (WIFEXITED(wait_status)) {
        result.status = SpawnResult::Status::Exited;
        result.exit_code = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        result.status = SpawnResult::Status::Signaled;
        result.signal_number = WTERMSIG(wait_status);
    }
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    return result;
}

#else // _WIN32

// UNVERIFIED: the local-only GUI tier is never compiled on CI, and this
// branch has never been compiled or run (macOS is the only verified host).
// It follows threat model R-16: CreateProcessW with lpApplicationName (no
// PATH search, no cmd.exe), lpCurrentDirectory for the working directory,
// inherited handles limited to the three standard handles through
// PROC_THREAD_ATTRIBUTE_HANDLE_LIST, and a Job Object with
// JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE so the child dies with the test.

namespace {

/// @brief Owns one Win32 HANDLE; closes it exactly once.
class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle) {}
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    ~UniqueHandle() { reset(); }

    [[nodiscard]] HANDLE get() const noexcept { return handle_; }
    [[nodiscard]] bool valid() const noexcept {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }
    HANDLE* out() noexcept {
        reset();
        return &handle_;
    }
    void reset(HANDLE handle = nullptr) noexcept {
        if (valid()) {
            ::CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_ = nullptr;
};

/// @brief UTF-8 -> UTF-16 for CreateProcessW.
std::wstring widen(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                          wide.data(), size);
    return wide;
}

/// @brief Appends one argument quoted per the documented
/// CommandLineToArgvW / MSVC CRT rules (backslashes doubled only before a
/// quote or the closing quote).
void append_quoted(std::wstring& command_line, const std::wstring& argument) {
    if (!command_line.empty()) {
        command_line += L' ';
    }
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        command_line += argument;
        return;
    }
    command_line += L'"';
    std::size_t backslashes = 0;
    for (const wchar_t c : argument) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'"') {
            command_line.append(backslashes * 2 + 1, L'\\');
        } else {
            command_line.append(backslashes, L'\\');
        }
        backslashes = 0;
        command_line += c;
    }
    command_line.append(backslashes * 2, L'\\');
    command_line += L'"';
}

} // namespace

SpawnResult spawn_and_capture(LiteralArg executable, std::span<const LiteralArg> arguments,
                              const std::filesystem::path& working_directory) {
    SpawnResult result;
    const std::filesystem::path executable_path(executable.c_str());
    if (!paths_are_valid(executable_path, working_directory)) {
        return result;
    }
    const std::wstring application = executable_path.wstring();
    std::wstring command_line;
    append_quoted(command_line, application);
    for (const LiteralArg& argument : arguments) {
        append_quoted(command_line, widen(argument.c_str()));
    }
    const std::wstring directory = working_directory.wstring();

    SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    UniqueHandle stdout_read;
    UniqueHandle stdout_write;
    UniqueHandle stderr_read;
    UniqueHandle stderr_write;
    if (!::CreatePipe(stdout_read.out(), stdout_write.out(), &inheritable, 0)
        || !::CreatePipe(stderr_read.out(), stderr_write.out(), &inheritable, 0)
        || !::SetHandleInformation(stdout_read.get(), HANDLE_FLAG_INHERIT, 0)
        || !::SetHandleInformation(stderr_read.get(), HANDLE_FLAG_INHERIT, 0)) {
        return result;
    }
    UniqueHandle null_input(::CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          &inheritable, OPEN_EXISTING, 0, nullptr));
    if (!null_input.valid()) {
        return result;
    }

    UniqueHandle job(::CreateJobObjectW(nullptr, nullptr));
    if (!job.valid()) {
        return result;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits,
                                   sizeof limits)) {
        return result;
    }

    // Inherit exactly the three standard handles, nothing else.
    HANDLE inherited[3] = {null_input.get(), stdout_write.get(), stderr_write.get()};
    SIZE_T attribute_size = 0;
    ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
    std::vector<unsigned char> attribute_storage(attribute_size);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
    if (!::InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size)) {
        return result;
    }
    struct AttributeListGuard {
        LPPROC_THREAD_ATTRIBUTE_LIST list;
        ~AttributeListGuard() { ::DeleteProcThreadAttributeList(list); }
    } attribute_guard{attributes};
    if (!::UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
                                     sizeof inherited, nullptr, nullptr)) {
        return result;
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof startup;
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = null_input.get();
    startup.StartupInfo.hStdOutput = stdout_write.get();
    startup.StartupInfo.hStdError = stderr_write.get();
    startup.lpAttributeList = attributes;

    PROCESS_INFORMATION process_info{};
    const auto start = Clock::now();
    if (!::CreateProcessW(application.c_str(), command_line.data(), nullptr, nullptr, TRUE,
                          CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW,
                          nullptr, directory.c_str(), &startup.StartupInfo, &process_info)) {
        return result;
    }
    UniqueHandle process(process_info.hProcess);
    UniqueHandle thread(process_info.hThread);
    if (!::AssignProcessToJobObject(job.get(), process.get())) {
        ::TerminateProcess(process.get(), 1);
        ::WaitForSingleObject(process.get(), INFINITE);
        return result;
    }
    ::ResumeThread(thread.get());
    stdout_write.reset();
    stderr_write.reset();
    null_input.reset();

    // One reader thread per pipe drains concurrently (no deadlock on a full
    // pipe); the shared counter enforces the cap and kills the job past it.
    std::atomic<std::size_t> total_bytes{0};
    std::atomic<bool> over_cap{false};
    const auto drain = [&](HANDLE pipe, std::string& sink) {
        char buffer[65536];
        DWORD n = 0;
        while (::ReadFile(pipe, buffer, sizeof buffer, &n, nullptr) && n > 0) {
            if (total_bytes.fetch_add(n) + n > kSpawnOutputCap) {
                over_cap = true;
                ::TerminateJobObject(job.get(), 1);
                return;
            }
            sink.append(buffer, n);
        }
    };
    std::thread stdout_reader(drain, stdout_read.get(), std::ref(result.stdout_bytes));
    std::thread stderr_reader(drain, stderr_read.get(), std::ref(result.stderr_bytes));

    const auto deadline_ms = static_cast<DWORD>(
        std::chrono::duration_cast<std::chrono::milliseconds>(kSpawnDeadline).count());
    const bool timed_out = ::WaitForSingleObject(process.get(), deadline_ms) == WAIT_TIMEOUT;
    if (timed_out) {
        ::TerminateJobObject(job.get(), 1);
        ::WaitForSingleObject(process.get(), INFINITE);
    }
    stdout_reader.join();
    stderr_reader.join();

    DWORD exit_code = 0;
    if (timed_out) {
        result.status = SpawnResult::Status::TimedOut;
    } else if (over_cap) {
        result.status = SpawnResult::Status::OutputLimitExceeded;
    } else if (::GetExitCodeProcess(process.get(), &exit_code)) {
        result.status = SpawnResult::Status::Exited;
        result.exit_code = static_cast<int>(exit_code);
    }
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    return result;
}

#endif // _WIN32

} // namespace smbios::test::gui
