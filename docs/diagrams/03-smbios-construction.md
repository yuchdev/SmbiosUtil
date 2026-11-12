# 3. Process: `SMBios` construction

What `SMBios::SMBios()` does (`libsmbios/src/smbios.cpp`). The source chain
is whatever the linked per-OS library's `make_default_source_chain()`
returns; the chain loop itself is platform-neutral. Mermaid, not rendered
locally.

## 3a. Constructor sequence

```mermaid
sequenceDiagram
    participant Caller
    participant B as SMBios
    participant F as make_default_source_chain
    participant S as ISmbiosSource, next in chain

    Caller->>B: SMBios()
    B->>F: make_default_source_chain()
    F-->>B: ordered vector of sources
    loop each source, until the first success
        B->>S: is_available()
        alt available
            B->>S: try_read(table_buffer_, major, minor)
            alt table read
                S-->>B: true
                B->>B: source_name_ = source_name(), stop
            else read failed
                S-->>B: false, try next source
            end
        else not available
            S-->>B: false, try next source
        end
    end
    B->>B: read_smbios_table()
    B-->>Caller: object ready, empty if every source failed
```

If every source fails nothing is thrown: `table_buffer_` stays empty and
`get_structures_count()` is `0`.

## 3b. Source order per platform

Highest priority first; each platform's definition lives in its per-OS
library (`libsmbios_linux/src/linux_source_chain.cpp`,
`libsmbios_macos/src/macos_source_chain.cpp`,
`libsmbios_win/src/windows_native_source.cpp`). Other `*nix` systems use
`libsmbios/src/fallback_source_chain.cpp`, which is not drawn here.

```mermaid
flowchart LR
    subgraph Linux
        L1["LinuxSysfsSource<br/>sysfs dmi tables"] --> L2["LinuxEfiSource<br/>efi systab"] --> L3["LegacyMemoryScanSource"]
    end
    subgraph macOS
        M1["MacOsUefiSource<br/>IOKit AppleSMBIOS"] --> M2["LegacyMemoryScanSource"]
    end
    subgraph Windows
        W1["WindowsNativeSource<br/>GetSystemFirmwareTable"] --> W2["LegacyMemoryScanSource"]
    end
```

## 3c. `read_smbios_table()` walk

```mermaid
flowchart TD
    start["read_smbios_table"] --> cnt["count_smbios_structures"]
    cnt --> nul{"table base null?"}
    nul -->|yes| done["return, no headers"]
    nul -->|no| nxt{"structures left<br/>by count?"}
    nxt -->|no| done
    nxt -->|yes| room{"4 header bytes<br/>before table end?"}
    room -->|no| done
    room -->|yes| hdr["read type, length, handle<br/>via read_u8 and read_u16"]
    hdr --> bad{"length under 4,<br/>or past table end?"}
    bad -->|yes| done
    bad -->|no| eot{"type is EndOfTable?"}
    eot -->|yes| done
    eot -->|no| push["push DMIHeader<br/>data points into table_buffer_"]
    push --> skip["skip formatted area, then<br/>string section to double NUL"]
    skip --> nxt
```
