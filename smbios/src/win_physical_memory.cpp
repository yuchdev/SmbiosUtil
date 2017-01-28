#if defined(_WIN32) || defined(_WIN64)
#include <smbios/win_physical_memory.h>
#include <smbios/win_native_api_helper.h>
#include <smbios/win_handle_ptr.h>

#include <memory>
#include <system_error>

using namespace smbios;

namespace {

HANDLE get_physical_memory_handle() {

    UNICODE_STRING memory_device_string;
    WCHAR physical_memory_device_name[] = L"\\device\\physicalmemory";

    HANDLE physical_memory_native_handle = INVALID_HANDLE_VALUE;
    RtlInitUnicodeString(&memory_device_string, physical_memory_device_name);

    OBJECT_ATTRIBUTES attributes{};
    InitializeObjectAttributes(&attributes, &memory_device_string, OBJ_CASE_INSENSITIVE, NULL, NULL);

    NTSTATUS status = NtOpenSection(&physical_memory_native_handle, SECTION_MAP_READ, &attributes);
    if (!NT_SUCCESS(status)) {
        throw std::system_error(GetLastError(), std::system_category());
    }
    return physical_memory_native_handle;
}

}

NativePhysicalMemory::NativePhysicalMemory()
{
}

NativePhysicalMemory::NativePhysicalMemory(size_t base, size_t length) 
    : physical_memory_device_(std::make_unique<WinHandlePtr>())
{
    map_physical_memory(base, length);
}

NativePhysicalMemory::~NativePhysicalMemory()
{
    unmap_memory();
}

void NativePhysicalMemory::map_physical_memory(size_t base, size_t length)
{
    // Load NTDLL entry points
    if (!is_ntdll_compatible()) {
        throw std::runtime_error("Unable to locate NTDLL entry points");
    }

    // Open physical memory
    physical_memory_device_->set_handle(get_physical_memory_handle());

    // Physical memory offset
    SYSTEM_INFO sysinfo{};
    GetSystemInfo(&sysinfo);
    page_offset_ = base % sysinfo.dwPageSize;

    // Map memory here
    PHYSICAL_ADDRESS physical_base{};
    physical_base.QuadPart = static_cast<ULONGLONG>(base);

    NTSTATUS map_view_status = NtMapViewOfSection(
        physical_memory_device_->handle(),      // Section handle
        INVALID_HANDLE_VALUE,                   // Process handle
        reinterpret_cast<PVOID*>(&virtual_address_), // BaseAddress (place the beginning of view here)
        0L,                                     // ZeroBits
        length,                                 // CommitSize
        &physical_base,                         // SectionOffset
        reinterpret_cast<PULONG>(&length),      // ViewSize
        ViewShare,                              // InheritDisposition
        0,                                      // AllocationFlags
        PAGE_READONLY);                         // Win32Protect

    if (!NT_SUCCESS(map_view_status)) {
        throw std::system_error(GetLastError(), std::system_category());
    }
}

bool NativePhysicalMemory::is_mapped() const
{
    return virtual_address_ && physical_memory_device_;
}

std::vector<uint8_t> NativePhysicalMemory::get_memory_dump(size_t offset, size_t length) const
{
    std::vector<uint8_t> memory_dump(length);
    uint8_t* aligned_begin = virtual_address_ - page_offset_;
    uint8_t* aligned_end = aligned_begin + length;
    memory_dump.assign(aligned_begin, aligned_end);
    return std::move(memory_dump);
}

uint8_t* NativePhysicalMemory::get_memory_offset(size_t offset) const
{
    return virtual_address_ - page_offset_ + offset;
}

bool NativePhysicalMemory::is_ntdll_compatible() const
{
    // RtlInitUnicodeString 
    FARPROC rtl_init_unicode_string = GetProcAddress(GetModuleHandle("ntdll.dll"), "RtlInitUnicodeString");
    if (RtlInitUnicodeString != reinterpret_cast<void*>(rtl_init_unicode_string)) {
        return false;
    }

    // NtMapViewOfSection
    FARPROC map_view_section = GetProcAddress(GetModuleHandle("ntdll.dll"), "NtMapViewOfSection");
    if (NtMapViewOfSection != reinterpret_cast<void*>(map_view_section)) {
        return false;
    }

    // NtUnmapViewOfSection
    FARPROC unmap_view_section = GetProcAddress(GetModuleHandle("ntdll.dll"), "NtUnmapViewOfSection");
    if (NtUnmapViewOfSection != reinterpret_cast<void*>(unmap_view_section)) {
        return false;
    }

    // NtOpenSection
    FARPROC open_section = GetProcAddress(GetModuleHandle("ntdll.dll"), "NtOpenSection");
    if (NtOpenSection != reinterpret_cast<void*>(open_section)) {
        return false;
    }
    return true;
}

void NativePhysicalMemory::unmap_memory()
{
    if (virtual_address_) {
        NtUnmapViewOfSection(INVALID_HANDLE_VALUE, virtual_address_);
        virtual_address_ = nullptr;
    }
}
#endif // defined(_WIN32) || defined(_WIN64)
