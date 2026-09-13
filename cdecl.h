#ifndef CDECL_HEADER_FILE
#define CDECL_HEADER_FILE

/*
 * Macros for the standard C calling convention.
 *
 * The macros work with every C and C++ compiler this course uses.
 *
 * Write a function prototype in this form:
 *
 *   return_type PRE_CDECL func_name(args) POST_CDECL;
 *
 * For example:
 *
 *   int PRE_CDECL f(int x, int y) POST_CDECL;
 */


#if defined(__GNUC__)
#  define PRE_CDECL
#  define POST_CDECL __attribute__((cdecl))
#else
#  define PRE_CDECL __cdecl
#  define POST_CDECL
#endif


#endif
