#pragma once

#if defined(_WIN32) || defined(_WIN64)

namespace smbios {

/// @brief RAII wrapper for Windows recourse handle
class WinHandlePtr {
public:

    /// @brief Empty (invalid) handle
    WinHandlePtr() = default;

    /// @brief Wrap handle
    WinHandlePtr(HANDLE value) : value_(value == INVALID_HANDLE_VALUE ? nullptr : value) {}

    /// @brief Set handle
    void set_handle(HANDLE value) {
        value_ = (value == INVALID_HANDLE_VALUE ? nullptr : value);
    }

    /// @brief Close handle
    ~WinHandlePtr()
    {
        ::CloseHandle(value_);
    }

    /// @brief Get raw handle
    HANDLE handle() { return value_; }

    explicit operator bool() const { return value_ != nullptr; }
    operator HANDLE() const { return value_; }

    friend bool operator ==(WinHandlePtr l, WinHandlePtr r) { return l.value_ == r.value_; }
    friend bool operator !=(WinHandlePtr l, WinHandlePtr r) { return !(l == r); }

    /// @brief For proper use with smart pointers
    struct Deleter {
        typedef WinHandlePtr pointer;
        void operator()(WinHandlePtr handle) const { ::CloseHandle(handle); }
    };
private:
    HANDLE value_ = INVALID_HANDLE_VALUE;
};

inline bool operator ==(HANDLE l, WinHandlePtr r) { return WinHandlePtr(l) == r; }
inline bool operator !=(HANDLE l, WinHandlePtr r) { return !(l == r); }
inline bool operator ==(WinHandlePtr l, HANDLE r) { return l == WinHandlePtr(r); }
inline bool operator !=(WinHandlePtr l, HANDLE r) { return !(l == r); }

} // namespace smbios

#endif // defined(_WIN32) || defined(_WIN64)
