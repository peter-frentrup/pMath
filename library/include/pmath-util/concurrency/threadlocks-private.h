#ifndef __PMATH_UTIL__CONCURRENCY__THREADLOCKS_PRIVATE_H__
#define __PMATH_UTIL__CONCURRENCY__THREADLOCKS_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

#include <pmath-config.h>
#include <pmath-types.h>

PMATH_PRIVATE void _pmath_threadlocks_memory_panic(void);

PMATH_PRIVATE pmath_bool_t _pmath_threadlocks_init(void);
PMATH_PRIVATE void         _pmath_threadlocks_done(void);

#endif /* __PMATH_UTIL__CONCURRENCY__THREADLOCKS_PRIVATE_H__ */
