#ifndef __PMATH_UTIL__HASHTABLES_PRIVATE_H__
#define __PMATH_UTIL__HASHTABLES_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-util/hash/hashtables.h>


struct _pmath_object_entry_t {
  pmath_t key;
  pmath_t value;
};

PMATH_PRIVATE extern const pmath_ht_class_ex_t pmath_ht_obj_class;

// for use with pmath_ht_copy():
PMATH_PRIVATE void *_pmath_object_entry_copy_func(void *entry); // for use with pmath_ht_copy()

/*============================================================================*/

struct _pmath_object_int_entry_t {
  pmath_t  key;
  intptr_t value;
};

PMATH_PRIVATE extern const pmath_ht_class_ex_t pmath_ht_obj_int_class;

// for use with pmath_ht_copy():
PMATH_PRIVATE void *_pmath_object_int_entry_copy_func(void *entry); // for use with pmath_ht_copy()

/*============================================================================*/

PMATH_PRIVATE unsigned int _pmath_hash_pointer(void *ptr);

/*============================================================================*/

#ifdef PMATH_DEBUG_TESTS
PMATH_PRIVATE void PMATH_TEST_NEW_HASHTABLES(void);
#else
#define PMATH_TEST_NEW_HASHTABLES() ((void)0)
#endif

#endif /* __PMATH_UTIL__HASHTABLES_PRIVATE_H__ */
