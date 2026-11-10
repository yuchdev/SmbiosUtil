#pragma once

#include <cstdint>
#include <vector>

#include <libsmbios/smbios.hpp>

// Test-support only (milestone 0009, Story 03.0, Task 03, decision D3): the
// fixed, synthetic SMBIOS table every output sample in
// docs/output-formats.md (and tests/data/output_formats/sample.*) is
// rendered from. Never linked into a library or an example.

namespace smbios::test {

/// @brief Builds the fixed, synthetic SMBIOS table used to generate every
/// output sample in docs/output-formats.md. Deterministic: same bytes on
/// every OS, every run. Contains no real hardware identifiers.
///
/// Contents (in order), built with SyntheticTableBuilder::add_structure():
/// - type 0 (BIOS Information): strings, exactly two set characteristic
///   bits, and a vendor string that exercises every format's escaping;
/// - type 11 (OEM Strings): a `uint64_t` string count and one string with a
///   leading `=` (CSV formula-injection prefix);
/// - type 32 (System Boot Information): a non-empty byte payload.
///
/// Privacy: no type 1/2/3/17 structure, and every string is a literal in
/// doc_sample_fixture.cpp - no byte is copied from a real machine.
/// @return the raw structure table, terminated by an End-of-Table marker,
/// ready for SmbiosTableWalkTestAccess::parse_synthetic_table().
std::vector<uint8_t> build_doc_sample_table();

/// @brief SMBIOS version the sample table is decoded against (3.4).
/// @return `SMBiosVersion{3, 4}`.
SMBiosVersion doc_sample_version();

} // namespace smbios::test
