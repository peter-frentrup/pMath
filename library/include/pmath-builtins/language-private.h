#ifndef __PMATH_BUILTINS__LANGUAGE_PRIVATE_H__
#define __PMATH_BUILTINS__LANGUAGE_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

#include <pmath-core/strings.h>

/* This header exports definitions of the sources in
   src/pmath-builtins/language/
 */

// PMATH_NULL on error
PMATH_PRIVATE pmath_t _pmath_parse_number(
  pmath_string_t  string, // will be freed
  pmath_bool_t alternative);

PMATH_PRIVATE pmath_bool_t _pmath_is_namespace(pmath_t name);
PMATH_PRIVATE pmath_bool_t _pmath_is_namespace_list(pmath_t list);

#endif /* __PMATH_BUILTINS__LANGUAGE_PRIVATE_H__ */
