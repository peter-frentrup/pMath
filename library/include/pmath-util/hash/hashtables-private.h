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

PMATH_PRIVATE extern const pmath_ht_class_t pmath_ht_obj_class;
PMATH_PRIVATE void                          pmath_ht_obj_class__entry_destructor(void *e);
PMATH_PRIVATE unsigned int                  pmath_ht_obj_class__entry_hash(void *e);
PMATH_PRIVATE pmath_bool_t                  pmath_ht_obj_class__entry_keys_equal(void *e1, void *e2);
PMATH_PRIVATE unsigned int                  pmath_ht_obj_class__key_hash(void *k);
PMATH_PRIVATE pmath_bool_t                  pmath_ht_obj_class__entry_equals_key(void *e, void *k);

// for use with pmath_ht_copy():
PMATH_PRIVATE void *_pmath_object_entry_copy_func(void *entry); // for use with pmath_ht_copy()

/*============================================================================*/

struct _pmath_object_int_entry_t {
  pmath_t  key;
  intptr_t value;
};

PMATH_PRIVATE extern const pmath_ht_class_t pmath_ht_obj_int_class;

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
