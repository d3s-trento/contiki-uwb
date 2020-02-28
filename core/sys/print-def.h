#ifndef PRINT_DEF_H
#define PRINT_DEF_H

/* Print a MACRO at compile time!
   Use a pragma directive as following:

   #pragma message STRDEF(YOUR_MACRO)
*/

#define _STRDEF_STR(x) #x
#define STRDEF(x) (#x ": " _STRDEF_STR(x))

/* Logging-related defines
 *
 * You may set LOG_PREFIX and LOG_LEVEL on a file-by-file basis.
 * Make sure to set them before including this header file.
 */

#define LOG_NONE 1          // no printing
#define LOG_ERR  2          // critical errors (cannot proceed)
#define LOG_WARN 3          // warinings (proceed but may fail)
#define LOG_DBG  4          // debug messages
#define LOG_INFO 5          // less important info
#define LOG_ALL LOG_INFO    // print it all

#ifndef LOG_PREFIX
#define LOG_PREFIX __FILE__ // file name by default
#endif


#if defined(LOG_LEVEL) && (LOG_LEVEL < LOG_NONE || LOG_LEVEL > LOG_ALL)
#error Log level set incorrectly: LOG_LEVEL
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_WARN  // warnings by default
#endif


#include <stdio.h>

#if LOG_LEVEL >= LOG_ERR
#define ERR(format, ...) do {printf("["LOG_PREFIX"] ERR:" format __VA_OPT__(,) __VA_ARGS__);} while(0)
#else
#define ERR(...) do {} while(0)
#endif

#if LOG_LEVEL >= LOG_WARN
#define WARN(format, ...) do {printf("["LOG_PREFIX"] WARN:" format __VA_OPT__(,) __VA_ARGS__);} while(0)
/* "Soft" assertions.
 * Use WARNIF(condition) to print a warning if the condition holds
 */
#define WARNIF(x) do {if (x) WARN("Check (" #x ") failed in %s (%s:%d)\n", __func__, __FILE__, __LINE__);} while(0)
#else
#define WARN(...) do {} while(0)
#define WARNIF(...) do {} while(0)
#endif

#if LOG_LEVEL >= LOG_DBG
#define DBG(format, ...) do {printf("["LOG_PREFIX"] DBG:" format __VA_OPT__(,) __VA_ARGS__);} while(0)
#define DBGF() do {printf("["LOG_PREFIX"] DBG: %s:%d\n", __func__, __LINE__ );} while(0)
#else
#define DBG(...) do {} while(0)
#define DBGF(...) do {} while(0)
#endif



#endif // PRINT_DEF_H


