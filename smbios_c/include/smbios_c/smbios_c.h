#ifndef SMBIOS_C_SMBIOS_C_H
#define SMBIOS_C_SMBIOS_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handles. smbios_handle_s/smbios_entry_handle_s are deliberately
 * never defined here - only this library's own .cpp sees the real type
 * behind them (a reinterpret_cast to/from smbios::SMBios* and the entry
 * types), so callers cannot dereference, copy by value, or inspect the
 * layout. */
typedef struct smbios_handle_s* smbios_handle_t;
typedef struct smbios_entry_handle_s* smbios_entry_handle_t;

typedef enum {
    SMBIOS_OK = 0,
    SMBIOS_ERR_NO_TABLE_FOUND = 1,   /* every ISmbiosSource failed */
    SMBIOS_ERR_INVALID_HANDLE = 2,
    SMBIOS_ERR_INVALID_ARGUMENT = 3,
    SMBIOS_ERR_BUFFER_TOO_SMALL = 4, /* caller-provided output buffer too small */
    SMBIOS_ERR_UNKNOWN = 99         /* an unexpected C++ exception was caught at the boundary */
} smbios_error_t;

/* Opens a handle onto the live machine's SMBIOS table (mirrors
 * smbios::SMBios()'s own source-chain walk). On success, writes a non-null
 * handle to *out_handle and returns SMBIOS_OK. On any failure, writes NULL
 * to *out_handle and returns a non-SMBIOS_OK code - callers may rely on a
 * null handle whenever the return is not SMBIOS_OK. Never throws across
 * this boundary: any C++ exception raised while opening is caught and
 * translated to smbios_error_t. */
smbios_error_t smbios_open(smbios_handle_t* out_handle);

/* Releases a handle obtained from smbios_open(). No-op on NULL. Never
 * fails and never throws - there is no error channel for a void-returning
 * cleanup function to report through. */
void smbios_close(smbios_handle_t handle);

/* Number of structures in the table opened by smbios_open(). */
smbios_error_t smbios_entry_count(smbios_handle_t handle, size_t* out_count);

/* Entry at the given 0-based index (< the count from smbios_entry_count()).
 * *out_entry is a non-owning view into data owned by the parent
 * smbios_handle_t - valid only until smbios_close() is called on that
 * parent handle, and never freed separately. Returns
 * SMBIOS_ERR_INVALID_ARGUMENT if index is out of range. */
smbios_error_t smbios_entry_at(smbios_handle_t handle, size_t index, smbios_entry_handle_t* out_entry);

/* Raw structure type/length/handle fields (DMIHeader::type/length/handle in
 * the C++ API). Return 0 if entry is NULL - these have no error channel,
 * so a caller must not pass a handle it has reason to believe is invalid. */
uint8_t smbios_entry_type(smbios_entry_handle_t entry);
uint8_t smbios_entry_length(smbios_entry_handle_t entry);
uint16_t smbios_entry_handle_id(smbios_entry_handle_t entry);

/* Renders a human-readable description of the entry into a caller-provided
 * buffer (the same POSIX strerror_r/readlink "caller-provided buffer"
 * pattern used throughout this API - see plan.md's Key constraints).
 * - If buf_len is large enough to hold the full string plus its null
 *   terminator: writes it into buf, sets *out_needed_len to the exact
 *   number of bytes written (string length + 1), and returns SMBIOS_OK.
 * - Otherwise: leaves buf untouched, sets *out_needed_len to the number
 *   of bytes a buffer would need to be (string length + 1) to succeed on
 *   a retry, and returns SMBIOS_ERR_BUFFER_TOO_SMALL. This also covers
 *   the common "ask how big a buffer I need" idiom of calling with
 *   buf == NULL, buf_len == 0.
 * If the entry's structure type is not one this library's factory
 * recognizes, writes a placeholder string ("Unknown structure type N")
 * rather than failing - a caller iterating every entry should not have to
 * special-case one unrecognized type among many known ones. */
smbios_error_t smbios_entry_render(smbios_entry_handle_t entry, char* buf, size_t buf_len, size_t* out_needed_len);

#ifdef __cplusplus
}
#endif

#endif /* SMBIOS_C_SMBIOS_C_H */
