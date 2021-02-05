#ifndef LOGGING_H
#define LOGGING_H

#include <stdint.h>

/*
 * You may set LOG_PREFIX and LOG_LEVEL on a file-by-file basis.
 * To do so, define them before including this header file.
 */

#define LOG_NONE 1          // no printing
#define LOG_ERR  2          // critical errors (cannot proceed)
#define LOG_WARN 3          // warinings (proceed but may fail)
#define LOG_INFO 4          // less important info
#define LOG_DBG  5          // detailed tracing for debugging
#define LOG_ALL LOG_DBG     // print it all

#ifndef LOG_PREFIX
#define LOG_PREFIX __FILE__ // file name by default
#endif


#if defined(LOG_LEVEL) && (LOG_LEVEL < LOG_NONE || LOG_LEVEL > LOG_ALL)
#error Log level set incorrectly: LOG_LEVEL
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_WARN  // warnings by default
#endif

#ifndef LOG_PRINTF
#define LOG_PRINTF(...) printf(__VA_ARGS__)
#endif

extern uint16_t logging_context;

#include <stdio.h>

#define __LOG_PRINT(format, level, ...) do {LOG_PRINTF("[" LOG_PREFIX " %u]" level format "\n", logging_context __VA_OPT__(,) __VA_ARGS__);} while(0)


#if LOG_LEVEL >= LOG_ERR
#define ERR(format, ...) __LOG_PRINT(format, "ERR:" __VA_OPT__(,) __VA_ARGS__)
#else
#define ERR(...) do {} while(0)
#endif

#if LOG_LEVEL >= LOG_WARN
#define WARN(format, ...) __LOG_PRINT(format, "WARN:" __VA_OPT__(,) __VA_ARGS__)
/* "Soft" assertions.
 * Use WARNIF(condition) to print a warning if the condition holds
 */
#define WARNIF(x) do {if (x) WARN("Check (" #x ") failed in %s (%s:%d)", __func__, __FILE__, __LINE__);} while(0)
#else
#define WARN(...) do {} while(0)
#define WARNIF(...) do {} while(0)
#endif

#if LOG_LEVEL >= LOG_INFO
#define INFO(format, ...) __LOG_PRINT(format, "" __VA_OPT__(,) __VA_ARGS__)
#else
#define INFO(...) do {} while(0)
#endif

#if LOG_LEVEL >= LOG_DBG
#define DBG(format, ...) __LOG_PRINT(format, "DBG:" __VA_OPT__(,) __VA_ARGS__)
#define DBGF() __LOG_PRINT("%s:%d", "DBG:", __func__, __LINE__ )
#else
#define DBG(...) do {} while(0)
#define DBGF(...) do {} while(0)
#endif

#define PRINT(format, ...) __LOG_PRINT(format, "" __VA_OPT__(,) __VA_ARGS__)

#endif //LOGGING_H
