/* ===========================================================================
 *  log.c — Logging implementation
 * =========================================================================== */
#include "common/log.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* ── State ── */
static LogLevel currentLevel = LOG_INFO;

/* ── Level names ── */
static const char *levelNames[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "SILENT"
};

/* ── Public API ── */

void logSetLevel(LogLevel level) {
    currentLevel = level;
}

LogLevel logGetLevel(void) {
    return currentLevel;
}

void logMessage(LogLevel level, const char *module, const char *fmt, ...) {
    if (level < currentLevel) {
        return;
    }

    /* Timestamp */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timeBuf[20];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", t);

    /* Print header */
    fprintf(stderr, "[%s] %-5s [%-4s] ",
            timeBuf, levelNames[level], module ? module : "----");

    /* Print formatted message */
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
    fflush(stderr);
}
