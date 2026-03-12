/* ===========================================================================
 *  error.h — Error handling utilities
 * =========================================================================== */
#ifndef M68K_ERROR_H
#define M68K_ERROR_H

#include "common/types.h"

/* Maximum length of an error message */
#define ERROR_MSG_MAX 256

/* Error context — stored per subsystem for detailed diagnostics */
typedef struct {
    ResultCode  code;
    char        message[ERROR_MSG_MAX];
    u32         address;     /* address that caused the error (if applicable) */
    u32         value;       /* value involved (if applicable) */
    const char *file;        /* source file where error occurred */
    int         line;        /* source line where error occurred */
} ErrorContext;

/* Set an error with context */
void errorSet(ErrorContext *ctx, ResultCode code, u32 address,
              const char *file, int line, const char *fmt, ...);

/* Clear an error context */
void errorClear(ErrorContext *ctx);

/* Check if an error is set */
bool errorIsSet(const ErrorContext *ctx);

/* Print an error context to stderr */
void errorPrint(const ErrorContext *ctx);

/* ── Convenience macro to capture __FILE__ and __LINE__ automatically ── */
#define ERROR_SET(ctx, code, addr, ...) \
    errorSet((ctx), (code), (addr), __FILE__, __LINE__, __VA_ARGS__)

#endif /* M68K_ERROR_H */
