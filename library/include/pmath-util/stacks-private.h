#ifndef __PMATH_UTIL__STACKS_PRIVATE_H__
#define __PMATH_UTIL__STACKS_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/stacks.h>
#include <pmath-types.h>


struct _pmath_stack_item_t {
  struct _pmath_stack_item_t  *next;
};

struct _pmath_stack_t {
  union {
    struct {
      struct _pmath_stack_item_t  *top;
      intptr_t  operation_counter_or_spinlock;
    } s;
    
    pmath_atomic2_t as_atomic2;
  } u;
};

/* A  struct _pmath_stack_t  must be aligned to 2*sizeof(void*),
   which is 8 on 32 bit system and 16 on 64 bit systems.
   However, dlmalloc() seems to guarantee only 8 bytes on my AMD64 Linux machine
*/
struct _pmath_unaligned_stack_t {
	char _data[sizeof(struct _pmath_stack_t) + 16];
};

PMATH_PRIVATE pmath_bool_t _pmath_stacks_init(void);

#endif /* __PMATH_UTIL__STACKS_PRIVATE_H__ */
