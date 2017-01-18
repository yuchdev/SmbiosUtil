#if defined(_WIN32) || defined(_WIN64)
#include <smbios_utility/win_system_information.h>
#include <Windows.h>
#include <tchar.h>


typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

std::map<unsigned short, std::string> NativeSystemInformation::cpu_architectures_ = {
    { PROCESSOR_ARCHITECTURE_AMD64, "AMD64"},
    { PROCESSOR_ARCHITECTURE_ARM, "ARM" },
    { PROCESSOR_ARCHITECTURE_IA64, "IA64-Itanium" },
    { PROCESSOR_ARCHITECTURE_INTEL, "Intel" },
    { PROCESSOR_ARCHITECTURE_UNKNOWN, "Unknown" }
};

bool is_x32_application_on_x64()
{

    BOOL is_wow_64 = FALSE;

    //IsWow64Process is not available on all supported versions of Windows.
    //Use GetModuleHandle to get a handle to the DLL that contains the function
    //and GetProcAddress to get a pointer to the function if available.

    LPFN_ISWOW64PROCESS fnIsWow64Process = 
        (LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");

    if(NULL != fnIsWow64Process){
        if (!fnIsWow64Process(GetCurrentProcess(), &is_wow_64)){
            //handle error
        }
    }
    return is_wow_64?true:false;
}

NativeSystemInformation::NativeSystemInformation()
{
    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);
    number_of_cpu_ = system_info.dwNumberOfProcessors;
    if (cpu_architectures_.end() != cpu_architectures_.find(system_info.wProcessorArchitecture)) {
        cpu_architecture_ = cpu_architectures_[system_info.wProcessorArchitecture];
    }

    get_system_version();
}

NativeSystemInformation::~NativeSystemInformation()
{
}

void NativeSystemInformation::get_system_version()
{
    OSVERSIONINFOEX info = {};
    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&info))) {
        retrived_successfully_ = false;
        os_name_ = "Unknown";
        cpu_architecture_ = "Unknown";
        return;
    }

    os_build_number_ = info.dwBuildNumber;
    os_major_version_ = info.dwMajorVersion;
    os_minor_version_ = info.dwMinorVersion;

    if (info.dwMajorVersion == 6 && info.dwMinorVersion == 0) {
        os_name_ = windows_vista_type(info.wProductType);
    }
    else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 1) {
        os_name_ = windows_7_type(info.wProductType);
    }
    else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 2) {
        os_name_ = windows_8_type(info.wProductType);
    }
    else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 3) {
        os_name_ = windows_81_type(info.wProductType);
    }
    else if (info.dwMajorVersion == 10 && info.dwMinorVersion == 0) {
        os_name_ = windows_10_type(info.wProductType);
    }
    else {
        os_name_ = "Unknown";
    }
}

std::string NativeSystemInformation::windows_vista_type(unsigned long product_type)
{
    if (product_type == VER_NT_WORKSTATION)
        return "Windows Vista";
    else
        return "Windows Server 2008";
}

std::string NativeSystemInformation::windows_7_type(unsigned long product_type)
{
    if (product_type == VER_NT_WORKSTATION)
        return "Windows 7";
    else
        return "Windows Server 2008 R2";
}

std::string NativeSystemInformation::windows_8_type(unsigned long product_type)
{
    if (product_type == VER_NT_WORKSTATION)
        return "Windows 8";
    else
        return "Windows Server 2012";
}

std::string NativeSystemInformation::windows_81_type(unsigned long product_type)
{
    if (product_type == VER_NT_WORKSTATION)
        return "Windows 8.1";
    else
        return "Windows Server 2008 R2";
}

std::string NativeSystemInformation::windows_10_type(unsigned long product_type)
{
    if (product_type == VER_NT_WORKSTATION)
        return "Windows 10";
    else
        return "Windows Server 2016";
}
#endif // defined(_WIN32) || defined(_WIN64)
