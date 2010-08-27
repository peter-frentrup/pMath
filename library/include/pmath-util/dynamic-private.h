#ifndef __PMATH_UTIL__DYNAMIC_PRIVATE_H__
#define __PMATH_UTIL__DYNAMIC_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

#include <pmath-core/symbols.h>

PMATH_PRIVATE extern volatile intptr_t _pmath_dynamic_trackers;

PMATH_PRIVATE void         _pmath_dynamic_bind(pmath_symbol_t symbol, intptr_t id); // symbol wont be freed
PMATH_PRIVATE pmath_bool_t _pmath_dynamic_remove(intptr_t id);
PMATH_PRIVATE void         _pmath_dynamic_update(pmath_symbol_t symbol); // symbol wont be freed

PMATH_PRIVATE pmath_bool_t _pmath_dynamic_init(void);
PMATH_PRIVATE void         _pmath_dynamic_done(void);

#endif /* __PMATH_UTIL__DYNAMIC_PRIVATE_H__ */
