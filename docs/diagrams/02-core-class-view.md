# 2. Components: core class view

Runtime classes of `libsmbios` and how they relate. Sources:
`libsmbios/include/libsmbios/smbios.hpp`, `smbios_entry_factory.hpp`,
`abstract_smbios_entry.hpp`, `output_formatter.hpp`, `field_value.hpp`,
and `sys_interface/include/smbios/sys_interface/`. Only a representative
subset of members and concrete classes is shown. Mermaid, not rendered
locally.

```mermaid
classDiagram
    direction LR

    class ISmbiosSource {
        <<interface>>
        +is_available()
        +try_read(out_table, major, minor)
        +source_name()
    }
    class LinuxSysfsSource
    class LinuxEfiSource
    class MacOsUefiSource
    class WindowsNativeSource
    class LegacyMemoryScanSource

    class SMBios {
        +SMBios()
        +begin()
        +end()
        +get_smbios_version()
        +get_table_base()
        +get_table_size()
        +render_to_description()
        -read_smbios_table()
    }
    class DMIHeader {
        +type
        +length
        +handle
        +data
    }

    class SMBiosEntryFactory {
        +create(header, version, table_end)
    }
    class AbstractSMBiosEntry {
        <<abstract>>
        +get_entry_size()
        +get_fields()*
        #dmi_string(index)
    }
    class BiosInformationEntry
    class NamedField {
        +name
        +value
    }

    class IOutputFormatter {
        <<interface>>
        +format_name()
        +render(entry)
        +begin_document()
        +entry_separator()
        +end_document()
    }
    class TextFormatter
    class JsonFormatter
    class XmlFormatter
    class YamlFormatter
    class CsvFormatter

    ISmbiosSource <|.. LinuxSysfsSource
    ISmbiosSource <|.. LinuxEfiSource
    ISmbiosSource <|.. MacOsUefiSource
    ISmbiosSource <|.. WindowsNativeSource
    ISmbiosSource <|.. LegacyMemoryScanSource

    SMBios ..> ISmbiosSource : walks make_default_source_chain
    SMBios "1" o-- "*" DMIHeader : headers_list_ views
    SMBiosEntryFactory ..> DMIHeader : reads
    SMBiosEntryFactory ..> AbstractSMBiosEntry : creates or nullptr
    AbstractSMBiosEntry <|-- BiosInformationEntry
    AbstractSMBiosEntry ..> NamedField : get_fields
    IOutputFormatter <|.. TextFormatter
    IOutputFormatter <|.. JsonFormatter
    IOutputFormatter <|.. XmlFormatter
    IOutputFormatter <|.. YamlFormatter
    IOutputFormatter <|.. CsvFormatter
    IOutputFormatter ..> AbstractSMBiosEntry : render
```

Registration seams (static-init registries, each invoked from the
implementing class's own `.cpp`, never a header):

```mermaid
flowchart LR
    e["entry .cpp<br/>REGISTER_SMBIOS_ENTRY"] -->|register_smbios_entry| reg1["entry registry<br/>smbios_entry_factory.cpp"]
    reg1 --> fac["SMBiosEntryFactory::create"]
    f["formatter .cpp<br/>REGISTER_OUTPUT_FORMATTER"] -->|register_output_formatter| reg2["formatter registry"]
    reg2 --> get["get_output_formatter<br/>registered_output_formatter_names"]
```

Notes:

- `LegacyMemoryScanSource` exists twice (one copy per legacy library,
  `libsmbios_win_legacy` and `libsmbios_posix_legacy`); only one is linked.
  Only the Linux, macOS and Windows chains are drawn in
  [diagram 3](03-smbios-construction.md).
- `CsvFormatter` lives in `smbios/examples/csv_formatter.cpp` and is
  compiled into the `smbios` CLI target, not `libsmbios`.
- `BiosInformationEntry` stands for every concrete entry class under
  `libsmbios/include/libsmbios/tables/`.
- `DMIHeader::data` points into `SMBios`'s raw table buffer, so entries
  are views whose lifetime is tied to the `SMBios` object.
