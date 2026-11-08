#pragma once
#include <vector>
#include <cstdint>
#include <string>

// Main SMBIOS table implementation

namespace smbios {

// should be aligned to be mapped to the physical memory
#pragma pack(push, 1)

/// @brief Each SMBIOS structure begins with that four-byte header
struct DMIHeader
{
    ///< Specifies the type of structure. Types 0 through 127 (7Fh) are
    ///< reserved for and defined by this specification (see
    ///< `SMBios::SMBiosHandler`). Types 128 through 256 (80h to FFh) are
    ///< available for system- and OEM-specific information.
    uint8_t type;

    ///< Specifies the length of the formatted area of the structure,
    ///< starting at the Type field (string section not included).
    uint8_t length;

    ///< Specifies the structure's handle, a unique 16-bit number in the
    ///< range 0 to 0FFFEh. If the system configuration changes, a
    ///< previously assigned handle might no longer exist!
    uint16_t handle;

    ///< Pointer to the entry's own formatted-area bytes (the Type field's
    ///< own address). Non-owning: it is a view into the `SMBios` object's
    ///< `table_buffer_` and is only valid for as long as that buffer (and
    ///< the owning `SMBios` instance) is alive.
    const uint8_t *data;

    /// @brief Printable size
    size_t get_length() const { return static_cast<size_t>(length); }

    /// @brief Printable type ID
    size_t get_type() const { return static_cast<size_t>(type); }
};

#pragma pack(pop)

/// @brief The SMBIOS specification version a table was produced against;
/// entry classes use this to decide which later-revision fields are safe to
/// read (e.g. `BiosInformationEntry`'s 2.4-vs-3.1 field set).
struct SMBiosVersion
{
    ///< Major SMBIOS specification revision number (e.g. `3` for "3.1").
    uint16_t major_version;

    ///< Minor SMBIOS specification revision number (e.g. `1` for "3.1").
    uint16_t minor_version;
};

/// @brief Greater-than comparison, major then minor (e.g. `{3, 0} > {2, 8}`
/// and `{2, 8} > {2, 7}`). `==`/`!=` are intentionally not provided: SMBIOS
/// is backward compatible, so callers need to know whether a table's
/// version is at least a given revision, not whether it is exactly equal.
/// @param lhs version to compare.
/// @param rhs version to compare against.
/// @return `true` if `lhs` is a strictly later SMBIOS revision than `rhs`.
bool operator >(const SMBiosVersion& lhs, const SMBiosVersion& rhs);

/// @brief Less-than comparison, major then minor; see `operator>` for why
/// `==`/`!=` are omitted.
/// @param lhs version to compare.
/// @param rhs version to compare against.
/// @return `true` if `lhs` is a strictly earlier SMBIOS revision than `rhs`.
bool operator <(const SMBiosVersion& lhs, const SMBiosVersion& rhs);

/// @brief Class owns system-independent SMBIOS table 
/// which however has been read using system-dependent API
/// It also have a cache like table structures count and headers
/// (it's obvious information could not be updated while computer is active)
/// Class support bidirectional iterators to be used in STL algorithms and cycles
class SMBios
{
public:

    /// @brief Structure type codes stored in `DMIHeader::type` (the name
    /// `SMBiosHandler` is this project's own, not the spec's). See the
    /// SMBIOS specification (http://www.dmtf.org/standards/smbios), table
    /// "Required structures and data", for the full registry; only the
    /// subset this project implements an `AbstractSMBiosEntry` subclass for
    /// is listed here. `EndOfTable` is the sentinel `read_smbios_table()`
    /// stops its walk at - see `SMBiosEntryFactory::create()` for how a
    /// value here maps to a concrete entry class.
    enum SMBiosHandler : uint8_t
    {
        BIOSInformation = 0,
        SystemInformation = 1,
        BaseboardInformation = 2,
        SystemEnclosure = 3,
        ProcessorInformation = 4,
        CacheInformation = 7,
        PortConnection = 8,
        SystemSlots = 9,
        OemStrings = 11,
        SystemConfigurationOptions = 12,
        FirmwareLanguageInformation = 13,
        GroupAssociations = 14,
        SystemEventLog = 15,
        PhysicalMemoryArray = 16,
        MemoryDevice = 17,
        MemoryErrorInformation32Bit = 18,
        MemoryArrayMappedAddress = 19,
        MemoryDeviceMappedAddress = 20,
        PointingDevice = 21,
        PortableBattery = 22,
        SystemReset = 23,
        HardwareSecurity = 24,
        SystemPowerControls = 25,
        VoltageProbe = 26,
        CoolingDevice = 27,
        TemperatureProbe = 28,
        ElectricalCurrentProbe = 29,
        OutOfBandRemoteAccess = 30,
        BootIntegrityServices = 31,
        SystemBootInformation = 32,
        MemoryErrorInformation64Bit = 33,
        ManagementDevice = 34,
        ManagementDeviceComponent = 35,
        ManagementDeviceThresholdData = 36,
        MemoryChannel = 37,
        IpmiDeviceInformation = 38,
        SystemPowerSupply = 39,
        AdditionalInformation = 40,
        OnboardDevicesExtendedInformation = 41,
        ManagementControllerHostInterface = 42,
        TpmDevice = 43,
        EndOfTable = 127
    };

    /// @brief Reads the live machine's SMBIOS table by walking this
    /// platform's `make_default_source_chain()` in order, stopping at the
    /// first source that is both available and successfully reads a table.
    /// If no source in the chain succeeds, this object is left with an
    /// empty table (`get_structures_count() == 0`, `begin() == end()`) and
    /// `source_name_` empty, rather than throwing - callers should check
    /// `get_structures_count()` if they need to distinguish "no SMBIOS
    /// data available" from "machine genuinely has zero structures".
    SMBios();

    /// @brief Destructor; empty body - every member is a self-managed
    /// `std::vector`/`std::string` with no manual cleanup needed.
    ~SMBios();

    /// @brief Get SMBIOS version
    SMBiosVersion get_smbios_version() const;

    /// @brief Count of structures found by `count_smbios_structures()`'s own
    /// independent walk of `table_buffer_`, computed once at construction.
    /// @note This walk has no special handling for `SMBiosHandler::EndOfTable`
    /// - it counts the marker itself and keeps counting any (possibly
    /// attacker-controlled) well-formed-looking bytes that follow it,
    /// unlike `read_smbios_table()`'s own walk (which populates
    /// `begin()`/`end()` and stops the instant it sees `EndOfTable`). The
    /// two can disagree on malformed/adversarial input - do not use this
    /// count as a substitute for `std::distance(begin(), end())` when the
    /// input's trustworthiness is not guaranteed.
    /// @return the raw structure count from this class's own walk, which
    /// may exceed the number of structures actually reachable via
    /// `begin()`/`end()`.
    size_t get_structures_count() const;

    /// @brief Actual table base (offset from header beginning)
    const uint8_t* get_table_base()  const;

    /// @brief Actual table size from table beginning (without header)
    size_t get_table_size()  const;

    /// @brief Renders a human-readable summary of this table (version,
    /// structure count, source name, and every structure's own
    /// `render_to_description()`) into a single multi-line string.
    std::string render_to_description() const;

    /// @brief Implement bidirectional iterator for STL-style processing
    class iterator {
    public:

        friend class SMBios;
        enum EndTag { end };

        iterator() {}

        iterator(std::vector<DMIHeader>& headers) :headers_list_(&headers) {
            pointed_header_ = headers_list_->begin();
        };

        iterator(std::vector<DMIHeader>& headers, EndTag) :headers_list_(&headers) {
            pointed_header_ = headers_list_->end();
        };

        const DMIHeader& operator*() {
            return *pointed_header_; 
        };

        const iterator& operator++() {
            ++pointed_header_;
            return *this;
        }

        const iterator& operator--() {
            --pointed_header_;
            return *this;
        }

        bool operator==(const iterator& it) const {
            return pointed_header_ == it.pointed_header_;
        }

        bool operator!=(const iterator& it) const {
            return pointed_header_ != it.pointed_header_;
        }

    private:
        std::vector<DMIHeader>* headers_list_ = nullptr;
        std::vector<DMIHeader>::iterator pointed_header_;
    };

    /// @brief Iterator begin - for STL-style processing
    iterator begin()
    {
        return iterator(get_headers_list());
    }

    /// @brief Iterator end - for STL-style processing
    iterator end()
    {
        return iterator(get_headers_list(), iterator::end);
    }

private:

    /// Friend-only access for the Story 03.0 bounds-checked table walk unit
    /// test (tests/unit/smbios_table_walk_test.cpp): lets the test inject a
    /// synthetic buffer directly into `table_buffer_` and re-invoke the
    /// private walk (`read_smbios_table()`) against it, without needing
    /// root/real hardware.
    friend class SmbiosTableWalkTestAccess;

    /// Friend-only access for iterator class
    std::vector<DMIHeader>& get_headers_list();

    /// Parse and save headers for every entry
    void read_smbios_table();

    /// Count all low-level structures stored in SMBios
    /// Do it once at start
    void count_smbios_structures();

private:

    /// Cached SMBIOS structures count
    size_t structures_count_ = 0;

    /// Cached major SMBIOS version
    size_t major_version_ = 0;

    /// Cached minor SMBIOS version
    size_t minor_version_ = 0;

    /// Raw SMBIOS structure-table bytes (header excluded), populated
    /// directly by whichever `ISmbiosSource::try_read()` in the chain
    /// (see `make_default_source_chain()`) succeeded first. `get_table_base()`/
    /// `get_table_size()` read from this exclusively - milestone 0005,
    /// Story 01.0 replaces the old three-branch `native_impl_`/
    /// `smbios_entry32_`/`smbios_entry64_` logic with this single buffer.
    std::vector<uint8_t> table_buffer_;

    /// Cached SMBIOS headers
    std::vector<DMIHeader> headers_list_;

    /// `ISmbiosSource::source_name()` of whichever source in the chain
    /// succeeded, or empty if none did. Surfaced by `render_to_description()`
    /// for diagnostics (plan.md R3: so a consumer can tell which path
    /// actually produced the data).
    std::string source_name_;
};

} // namespace smbios
