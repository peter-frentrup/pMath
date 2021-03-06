#ifndef __PMATH_BUILTINS__SYMBOLS_PRIVATE_H__
#define __PMATH_BUILTINS__SYMBOLS_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-builtins/all-symbols.h>

struct _pmath_thread_t;


/* EARLYCALL is like DOWNCALL, except that the builtin code is run before any
   user-defined rule. So one cannot override the default function implementation
   e.g. with Unprotect(Plus);2+2:= 5
   But the main reason for EARLYCALL is the performance boost for Plus and Times
   with numbers (which comes at the additional cost of testing for the
   availability of EARLYCALL code for every other function) 
 */
#define PMATH_CODE_USAGE_EARLYCALL ((pmath_code_usage_t)3)


/* PMATH_CODE_USAGE_APPROX uses a different calling convention than the other
   PMATH_CODE_USAGE_XXX.
 */
#define PMATH_CODE_USAGE_APPROX ((pmath_code_usage_t)4)


PMATH_PRIVATE pmath_bool_t _pmath_symbol_builtins_init(void);
PMATH_PRIVATE void _pmath_symbol_builtins_protect_all(void);
PMATH_PRIVATE void _pmath_symbol_builtins_done(void);

#endif /* __PMATH_BUILTINS__SYMBOLS_PRIVATE_H__ */
