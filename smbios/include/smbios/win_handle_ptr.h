#pragma once

#if defined(_WIN32) || defined(_WIN64)

class WinHandlePtr {
public:
    WinHandlePtr() : value_() {}

    WinHandlePtr(HANDLE value) : value_(value == INVALID_HANDLE_VALUE ? nullptr : value) {}

    void set_handle(HANDLE value) {
        value_ = (value == INVALID_HANDLE_VALUE ? nullptr : value);
    }

    ~WinHandlePtr()
    {
        ::CloseHandle(value_);
    }

    HANDLE handle() { return value_; }

    explicit operator bool() const { return value_ != nullptr; }
    operator HANDLE() const { return value_; }

    friend bool operator ==(WinHandlePtr l, WinHandlePtr r) { return l.value_ == r.value_; }
    friend bool operator !=(WinHandlePtr l, WinHandlePtr r) { return !(l == r); }

    struct Deleter {
        typedef WinHandlePtr pointer;
        void operator()(WinHandlePtr handle) const { ::CloseHandle(handle); }
    };
private:
    HANDLE value_ = nullptr;
};

inline bool operator ==(HANDLE l, WinHandlePtr r) { return WinHandlePtr(l) == r; }
inline bool operator !=(HANDLE l, WinHandlePtr r) { return !(l == r); }
inline bool operator ==(WinHandlePtr l, HANDLE r) { return l == WinHandlePtr(r); }
inline bool operator !=(WinHandlePtr l, HANDLE r) { return !(l == r); }

#endif // defined(_WIN32) || defined(_WIN64)
