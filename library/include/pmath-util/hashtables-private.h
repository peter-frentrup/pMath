#ifndef __PMATH_UTIL__NEW_HASHTABLES_PRIVATE_H__
#define __PMATH_UTIL__NEW_HASHTABLES_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

typedef struct _pmath_hashtable_t *pmath_hashtable_t; // not threadsafe

typedef void (*pmath_ht_entry_callback_t)(void *entry, void *data);
  
typedef void *(*pmath_ht_entry_copy_t)(void *entry);
  
typedef unsigned int (*pmath_ht_entry_hash_func_t)(void *entry);
  
typedef unsigned int (*pmath_ht_key_hash_func_t)(void *key);

typedef pmath_bool_t (*pmath_ht_entry_equal_func_t)(
  void *entry1, 
  void *entry2);

typedef pmath_bool_t (*pmath_ht_entry_equals_key_func_t)(
  void *entry,
  void *key);

typedef struct{
  pmath_callback_t                  entry_destructor;
  pmath_ht_entry_hash_func_t        entry_hash;
  pmath_ht_entry_equal_func_t       entry_keys_equal;
  pmath_ht_key_hash_func_t          key_hash;
  pmath_ht_entry_equals_key_func_t  entry_equals_key;
}pmath_ht_class_t;

/*============================================================================*/

PMATH_PRIVATE pmath_hashtable_t pmath_ht_create(
  const pmath_ht_class_t  *klass,
  unsigned int             minsize);

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_hashtable_t pmath_ht_copy(
  pmath_hashtable_t      ht,
  pmath_ht_entry_copy_t  entry_copy);

PMATH_PRIVATE void pmath_ht_destroy(pmath_hashtable_t ht);

PMATH_PRIVATE void pmath_ht_clear(pmath_hashtable_t ht);
  
PMATH_PRIVATE unsigned int pmath_ht_capacity(pmath_hashtable_t ht);
PMATH_PRIVATE unsigned int pmath_ht_count(pmath_hashtable_t ht);

PMATH_PRIVATE void *pmath_ht_entry(pmath_hashtable_t ht, unsigned int i);
  
PMATH_PRIVATE 
void *pmath_ht_search(pmath_hashtable_t ht, void *key);
  
// You must free the result
PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
void *pmath_ht_insert(pmath_hashtable_t ht, void *entry);

// You must free the result
PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
void *pmath_ht_remove(pmath_hashtable_t ht, void *key);

/*============================================================================*/

struct _pmath_object_entry_t{
  pmath_t key;
  pmath_t value;
};

PMATH_PRIVATE extern const pmath_ht_class_t pmath_ht_obj_class;

// for use with pmath_ht_copy():
PMATH_PRIVATE void *_pmath_object_entry_copy_func(void *entry); // for use with pmath_ht_copy()

/*============================================================================*/

struct _pmath_object_int_entry_t{
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

#endif /* __PMATH_UTIL__NEW_HASHTABLES_PRIVATE_H__ */
