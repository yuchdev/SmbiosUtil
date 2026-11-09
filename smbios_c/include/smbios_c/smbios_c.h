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

#ifdef __cplusplus
}
#endif

#endif /* SMBIOS_C_SMBIOS_C_H */
