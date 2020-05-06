#ifndef PRINT_DEF_H
#define PRINT_DEF_H

/* Print a MACRO at compile time!
   Use a pragma directive as following:

   #pragma message STRDEF(YOUR_MACRO)
*/

#define _STRDEF_STR(x) #x
#define STRDEF(x) (#x ": " _STRDEF_STR(x))

/* #define _ENQUOTE(x) #x */
/* #define _VALUE(x) _ENQUOTE(x) */
/* #define STRDEF(x) (#x ": " _VALUE(x)) */


#endif // PRINT_DEF_H


