/* ===========================================================================
 *  log.h — Simple logging system with levels and module tags
 * =========================================================================== */
#ifndef M68K_LOG_H
#define M68K_LOG_H

#include "common/types.h"

/* Log levels (ascending severity) */
typedef enum {
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_SILENT    /* disables all output */
} LogLevel;

/* Set the minimum log level that will be printed */
void logSetLevel(LogLevel level);

/* Get the current log level */
LogLevel logGetLevel(void);

/* Core logging function — use the macros below instead */
void logMessage(LogLevel level, const char *module, const char *fmt, ...);

/* ── Convenience macros ── */
#define LOG_TRACE(mod, ...) logMessage(LOG_TRACE, (mod), __VA_ARGS__)
#define LOG_DEBUG(mod, ...) logMessage(LOG_DEBUG, (mod), __VA_ARGS__)
#define LOG_INFO(mod, ...)  logMessage(LOG_INFO,  (mod), __VA_ARGS__)
#define LOG_WARN(mod, ...)  logMessage(LOG_WARN,  (mod), __VA_ARGS__)
#define LOG_ERROR(mod, ...) logMessage(LOG_ERROR, (mod), __VA_ARGS__)
#define LOG_FATAL(mod, ...) logMessage(LOG_FATAL, (mod), __VA_ARGS__)

/* ── Module name constants ── */
#define MOD_CPU    "CPU"
#define MOD_BUS    "BUS"
#define MOD_MEM    "MEM"
#define MOD_DEV    "DEV"
#define MOD_ASM    "ASM"
#define MOD_DIS    "DIS"
#define MOD_CC     "CC"
#define MOD_GUI     "GUI"
#define MOD_MACHINE "MACHINE"
#define MOD_MAIN    "MAIN"

#endif /* M68K_LOG_H */
