#ifndef PRINT_DEF_H
#define PRINT_DEF_H

/*
    Print a MACRO at compile time!
    Use a pragma directive as following:

    #pragma message STRDEF(YOUR_MACRO)

*/


#define _STRDEF_STR(x) #x
#define STRDEF(x) (#x ": " _STRDEF_STR(x))

/* "Soft" assertions.
 *
 * Use WARNIF(condition) to print a warning if the condition holds
 *
 * Enable it in a per-file basis by defining ENABLE_WARNIF prior to
 * including this header file.
 */

#ifdef ENABLE_WARNIF
#define WARNIF(x) do {if (x) printf("Check (" #x ") failed in %s (%s:%d)\n", __func__, __FILE__, __LINE__);} while(0)
#else
#define WARNIF(x) do {} while(0)
#endif

#endif // PRINT_DEF_H


