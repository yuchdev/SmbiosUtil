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

#ifdef __cplusplus
}
#endif

#endif /* SMBIOS_C_SMBIOS_C_H */
