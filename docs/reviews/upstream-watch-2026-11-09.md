# Upstream watch - 2026-11-09

## Scope
| Upstream | baseline_sha | Reviewed HEAD | Commits in range | Fetch result |
|----------|--------------|---------------|------------------|--------------|
| dmidecode_linux | b70f6ee0f2e879deebc630da20d27d350407889e | b70f6ee0f2e879deebc630da20d27d350407889e | 0 | b70f6ee0f2e879deebc630da20d27d350407889e |
| dmidecode_macos | 334e53aae57632329adf11fe5e852749b038a793 | 334e53aae57632329adf11fe5e852749b038a793 | 0 | 334e53aae57632329adf11fe5e852749b038a793 |

- Fetch script exit status: 0. Both baselines verified reachable (commit exists, ancestor of HEAD).
- Windows documentation: https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemfirmwaretable, fetched 2026-11-09 (no redirect, no 404).

## Findings
None. No upstream commits in either range. The Windows documentation
baseline is recorded below in the digest; it is not an upstream change, so it
is not a finding. Our files that depend on that page are
`libsmbios_win/include/smbios/windows/win_bios.hpp`,
`libsmbios_win/src/win_bios.cpp` and
`libsmbios_win/src/windows_native_source.cpp`.

## Process issues
None.

## Windows documentation digest
- **Last updated:** ms.date 2018-12-05; updated_at 2025-12-18.
- **Signature:** `UINT GetSystemFirmwareTable(DWORD FirmwareTableProviderSignature, DWORD FirmwareTableID, PVOID pFirmwareTableBuffer, DWORD BufferSize)`; returns UINT.
- **Providers:** 'ACPI', 'FIRM', 'RSMB'.
- **RSMB buffer layout:** Used20CallingMethod, SMBIOSMajorVersion, SMBIOSMinorVersion, DmiRevision, Length, SMBIOSTableData[].
- **Return-value semantics:** Success returns bytes written (at most BufferSize); a too-small buffer returns the required size (greater than BufferSize); any other failure returns zero with GetLastError available.
- **Remarks topics:** UWP access via the `smbios` restricted capability (Windows 10 1803+, RSMB only); \Device\PhysicalMemory is kernel-mode only since Server 2003 SP1; WMI MSSMBios_RawSMBiosTables as an alternative; FIRM returns a physical address range; ACPI returns only the first of same-named tables (use EnumSystemFirmwareTables first); table ID is little-endian (reversed characters).
- **Requirements:** Client Windows Vista / XP Pro x64; server Windows Server 2008 / 2003 SP1; header sysinfoapi.h (include Windows.h); Kernel32.lib; Kernel32.dll.

First run: baseline recorded.

## Verdict
- dmidecode_linux: fully triaged (empty range).
- dmidecode_macos: fully triaged (empty range).
- Findings (upstream changes only): Cosmetic 0, New information 0, Correctness 0. No process issues.
- Report conformed by hand to the revised agent template (2026-11-09 security/feature review): the
  trial run used the definition as first registered.
- This report does not block any merge.

## Recommended actions
- dmidecode_linux: no advance needed (baseline equals reviewed HEAD).
- dmidecode_macos: no advance needed (baseline equals reviewed HEAD).
- Windows documentation: no action; this digest is the baseline for future runs.
