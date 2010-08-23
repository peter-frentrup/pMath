#ifndef __PMATH_UTIL__STACKS_PRIVATE_H__
#define __PMATH_UTIL__STACKS_PRIVATE_H__

#include <pmath-util/stacks.h>
#include <pmath-types.h>

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

struct _pmath_stack_item_t{
  struct _pmath_stack_item_t  *next;
};

struct _pmath_stack_t{
  struct _pmath_stack_item_t  *top;
  intptr_t  operation_counter_or_spinlock;
};

PMATH_PRIVATE pmath_bool_t _pmath_stacks_init(void);

#endif /* __PMATH_UTIL__STACKS_PRIVATE_H__ */
