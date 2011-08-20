#ifndef __PMATH_BUILTINS__PARALLEL_PRIVATE_H__
#define __PMATH_BUILTINS__PARALLEL_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

/* This header exports all definitions of the sources in
   src/pmath-builtins/parallel/
 */

// for pmath_custom_t where data is pmath_task_t:
PMATH_PRIVATE void _pmath_custom_task_destroy(void *data);

#endif // __PMATH_BUILTINS__PARALLEL_PRIVATE_H__
