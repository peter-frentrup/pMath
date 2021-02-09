#ifndef __PMATH_BUILTINS__LANGUAGE_PRIVATE_H__
#define __PMATH_BUILTINS__LANGUAGE_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

#include <pmath-core/strings.h>

/* This header exports definitions of the sources in
   src/pmath-builtins/language/
 */
  
// evaluates MakeExpression(box) but also retains debug information
PMATH_PRIVATE pmath_t _pmath_makeexpression_with_debuginfo(pmath_t box);

PMATH_PRIVATE pmath_bool_t _pmath_is_machinenumber(pmath_t x);

#endif /* __PMATH_BUILTINS__LANGUAGE_PRIVATE_H__ */
