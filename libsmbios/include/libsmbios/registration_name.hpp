#pragma once

/// @file registration_name.hpp
/// @brief Preprocessor helper shared by the self-registration macros
/// (REGISTER_SMBIOS_ENTRY in smbios_entry_factory.hpp and
/// REGISTER_OUTPUT_FORMATTER in output_formatter.hpp). Not part of the
/// documented extension API; use those two macros instead.

/// @brief Token-pastes `a` and `b` after macro-expanding both (the extra
/// level of indirection is what lets `__COUNTER__` expand before `##`).
#define SMBIOS_DETAIL_CONCAT_IMPL(a, b) a##b

/// @brief Macro-expanding token paste; see SMBIOS_DETAIL_CONCAT_IMPL.
#define SMBIOS_DETAIL_CONCAT(a, b) SMBIOS_DETAIL_CONCAT_IMPL(a, b)

/// @brief A namespace-scope identifier that is unique within the current
/// translation unit, built from `prefix` and `__COUNTER__` (supported by
/// GCC, Clang and MSVC). The registration macros use it instead of pasting
/// the class name, so a namespace-qualified class name such as
/// `ns::CsvFormatter` - which cannot be part of an identifier - works.
/// Each use yields a fresh name, so several registrations may share one
/// translation unit (and even one source line).
#define SMBIOS_DETAIL_UNIQUE_NAME(prefix) SMBIOS_DETAIL_CONCAT(prefix, __COUNTER__)
