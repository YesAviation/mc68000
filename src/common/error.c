/* ===========================================================================
 *  error.c — Error handling implementation
 * =========================================================================== */
#include "common/error.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void errorSet(ErrorContext *ctx, ResultCode code, u32 address,
              const char *file, int line, const char *fmt, ...) {
    if (!ctx) return;

    ctx->code    = code;
    ctx->address = address;
    ctx->file    = file;
    ctx->line    = line;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->message, ERROR_MSG_MAX, fmt, args);
    va_end(args);
}

void errorClear(ErrorContext *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(ErrorContext));
    ctx->code = RESULT_OK;
}

bool errorIsSet(const ErrorContext *ctx) {
    return ctx && ctx->code != RESULT_OK;
}

void errorPrint(const ErrorContext *ctx) {
    if (!ctx || ctx->code == RESULT_OK) return;

    static const char *codeNames[] = {
        "OK", "ERROR", "BUS_ERROR", "ADDRESS_ERROR",
        "ILLEGAL_INSTRUCTION", "PRIVILEGE_VIOLATION",
        "HALTED", "DOUBLE_FAULT"
    };

    const char *name = (ctx->code < sizeof(codeNames) / sizeof(codeNames[0]))
                       ? codeNames[ctx->code] : "UNKNOWN";

    fprintf(stderr, "Error [%s] at $%06X: %s\n", name, ctx->address, ctx->message);
    if (ctx->file) {
        fprintf(stderr, "  (raised at %s:%d)\n", ctx->file, ctx->line);
    }
}
