#pragma once
#include <memory>
#include <map>
#include <string>
#if defined(_WIN32) || defined(_WIN64)

namespace smbios {

/// @brief Whether this binary was itself compiled as a 64-bit (`_M_X64`)
/// target - a compile-time check of this build, not a runtime check of
/// the host OS.
/// @return `true` if compiled for x64 (`_M_X64` defined); `false` otherwise.
constexpr bool is_x64_application()
{
#ifdef _M_X64
    return true;
#else
    return false;
#endif
}


/// @brief Runtime check for whether this (32-bit) process is running
/// under WOW64 on a 64-bit Windows host, via `IsWow64Process` resolved
/// dynamically through `GetProcAddress` (not statically linked, since
/// that API is absent on some older supported Windows versions).
/// @return `true` if `IsWow64Process` exists and reports this process is
/// running under WOW64; `false` if the API is unavailable, the call
/// fails, or this process is a native (non-WOW64) process.
bool is_x32_application_on_x64();

/// @brief Collect system-dependent OS and architecture information
/// from WinAPI call GetSystemInfo() and NTDLL's RtlGetVersion()
class NativeSystemInformation{
public:

    /// @brief Collect available system information
    NativeSystemInformation();

    /// @brief Make compiler happy
    ~NativeSystemInformation();

    /// @brief logical number of CPU (cores)
    int number_of_cpu() const
    {
        return number_of_cpu_;
    }

    /// @brief Windows build number
    int build_number() const
    {
        return os_build_number_;
    }

    /// @brief Windows major version
    int major_version() const
    {
        return os_major_version_;
    }

    /// @brief Windows minor version
    int minor_version() const
    {
        return os_minor_version_;
    }

    /// @brief String representation of CPU architecture
    /// "Unknown" if impossible to detect
    const std::string& cpu_architecture() const
    {
        return cpu_architecture_;
    }

    /// @brief String representation of OS name and major version
    /// "Unknown" if impossible to detect
    const std::string& os_name() const
    {
        return os_name_;
    }

    /// @brief Return true if OS information is retrieved successfully
    /// Just for caller 
    bool info_retrived_successfully() const 
    {
        return retrived_successfully_;
    }

private:

    /// @brief Convert major.minor OS version to string
    void get_system_version();

    /// @brief Converts Windows Vista versions to string
    /// @param product_type `OSVERSIONINFOEX::wProductType`
    /// (`VER_NT_WORKSTATION`/`VER_NT_DOMAIN_CONTROLLER`/`VER_NT_SERVER`),
    /// identifying client vs. server editions for this major/minor OS
    /// version.
    std::string windows_vista_type(unsigned long product_type);

    /// @brief Converts Windows 7 versions to string; see
    /// windows_vista_type() for `product_type`'s meaning.
    std::string windows_7_type(unsigned long product_type);

    /// @brief Converts Windows 8 versions to string; see
    /// windows_vista_type() for `product_type`'s meaning.
    std::string windows_8_type(unsigned long product_type);

    /// @brief Converts Windows 8.1 versions to string; see
    /// windows_vista_type() for `product_type`'s meaning.
    std::string windows_81_type(unsigned long product_type);

    /// @brief Converts Windows 10 versions to string; see
    /// windows_vista_type() for `product_type`'s meaning.
    std::string windows_10_type(unsigned long product_type);

private:

    /// Class state flag
    /// NativeSystemInformation exists even if not all information has been retrieved
    bool retrived_successfully_ = true;

    /// Extracted from SYSTEM_INFO
    unsigned long number_of_cpu_ = 0;

    /// Extracted from SYSTEM_INFO
    std::string cpu_architecture_;

    /// Extracted from OSVERSIONINFOEX 
    std::string os_name_;

    /// Extracted from OSVERSIONINFOEX 
    unsigned long os_build_number_;

    /// Extracted from OSVERSIONINFOEX 
    unsigned long os_major_version_;

    /// Extracted from OSVERSIONINFOEX 
    unsigned long os_minor_version_;

    /// Map major.minor OS version to string
    static std::map<unsigned short, std::string> cpu_architectures_;
};

} // namespace smbios

#endif
