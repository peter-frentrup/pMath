#include <pmath-util/hashtables-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/memory.h>

#include <limits.h>
#include <string.h>


#define HT_MINSIZE   (8)  /* power of 2, >= 2 */

#define DELETED_ENTRY  ((void*)UINTPTR_MAX) /* 0xFFF...FF = -1 */

#define IS_USED_ENTRY(e)  (((uintptr_t)(e))+1U > 1U) /* ((e) && (e) != DELETED_ENTRY) */

struct _pmath_hashtable_t {
  const pmath_ht_class_t *klass;

  // allways: used_count <= nonnull_count < capacity = 2**n = length of table
  unsigned int   nonnull_count;
  unsigned int   used_count;
  unsigned int   capacity;

  void         **table;
  void          *small_table[HT_MINSIZE];
};

static unsigned int lookup(
  pmath_hashtable_t   ht,
  void               *key_or_entry,
  unsigned int      (*hash)(void*),
  pmath_bool_t      (*entry_equals_key)(void*, void*)
) {
  unsigned int freeslot = UINT_MAX;
  unsigned int h, index;

  assert(ht != NULL);

  h = hash(key_or_entry);
  index = h & (ht->capacity - 1);

  for(;;) {
    if(!ht->table[index]) {
      if(freeslot == UINT_MAX)
        return index;
      return freeslot;
    }

    if(ht->table[index] == DELETED_ENTRY) {
      if(freeslot == UINT_MAX)
        freeslot = index;
    }
    else if(entry_equals_key(ht->table[index], key_or_entry))
      return index;

    index = (5 * index + 1 + h) & (ht->capacity - 1);
    h >>= 5;
  }
}

static pmath_bool_t resize(pmath_hashtable_t ht, unsigned int minused) {
  unsigned int newsize, i;
  void **entry_ptr;
  void **oldtable;

  if(!ht)
    return FALSE;

  assert(ht->klass != NULL);

  for(newsize = HT_MINSIZE; newsize <= minused && 0 < (int)newsize; newsize <<= 1) {
  }

  if(newsize == ht->capacity)
    return TRUE;

  if((int)newsize <= 0) {
    // cause "memory panic" => throw pMath exception/...
    pmath_mem_free(pmath_mem_alloc(SIZE_MAX));
    return FALSE;
  }

  oldtable = ht->table;
  if(newsize > HT_MINSIZE) {
    void **newtable = pmath_mem_alloc(newsize * sizeof(void*));

    if(!newtable)
      return FALSE;

    ht->table = newtable;
  }
  else
    ht->table = ht->small_table;

  assert(ht->table != oldtable);

//  oldsize      = ht->capacity;
  ht->capacity = newsize;
//  ht->mask     = newsize - 1;
  memset(ht->table, 0, newsize * sizeof(void*));

  i = ht->used_count;
  for(entry_ptr = oldtable; i > 0; ++entry_ptr) {
    if(IS_USED_ENTRY(*entry_ptr)) {
      int i2 = lookup(
                 ht,
                 *entry_ptr,
                 ht->klass->entry_hash,
                 ht->klass->entry_keys_equal);

      --i;
      assert(ht->table[i2] == NULL);

      ht->table[i2] = *entry_ptr;
    }
  }

  if(oldtable != ht->small_table)
    pmath_mem_free(oldtable);

  ht->nonnull_count = ht->used_count;

  return TRUE;
}

/*============================================================================*/

PMATH_API pmath_hashtable_t pmath_ht_create(
  const pmath_ht_class_t  *klass,
  unsigned int             minsize
) {
  pmath_hashtable_t ht;

  assert(klass != NULL);

  if(minsize > (1 << 30))
    return NULL;

  ht = pmath_mem_alloc(sizeof(struct _pmath_hashtable_t));
  if(!ht)
    return NULL;

  ht->klass         = klass;
  ht->nonnull_count = 0;
  ht->used_count    = 0;
  ht->capacity      = HT_MINSIZE;
  ht->table         = ht->small_table;
  memset(ht->table, 0, ht->capacity * sizeof(void*));

  if(!resize(ht, minsize)) {
    pmath_ht_destroy(ht);
    return NULL;
  }

  return ht;
}

/*============================================================================*/

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_hashtable_t pmath_ht_copy(
  pmath_hashtable_t      ht,
  pmath_ht_entry_copy_t  entry_copy
) {
  pmath_hashtable_t result;
  unsigned int i;

  if(!ht)
    return NULL;

  assert(ht->klass  != NULL);
  assert(entry_copy != NULL);

  result = pmath_ht_create(ht->klass, 0);
  if(!resize(result, ht->capacity)) {
    pmath_ht_destroy(result);
    return NULL;
  }

  assert(ht->capacity == result->capacity);

  for(i = 0; i <= result->capacity; ++i) {
    if(IS_USED_ENTRY(ht->table[i]))
      result->table[i] = entry_copy(ht->table[i]);
  }

  result->used_count    = ht->used_count;
  result->nonnull_count = ht->nonnull_count;

  return result;
}

/*============================================================================*/

PMATH_API void pmath_ht_destroy(pmath_hashtable_t ht) {
  unsigned int i;

  if(!ht)
    return;

  assert(ht->klass != NULL);

  for(i = 0; i < ht->capacity; ++i)
    if(IS_USED_ENTRY(ht->table[i]))
      ht->klass->entry_destructor(ht->table[i]);

  if(ht->table != ht->small_table)
    pmath_mem_free(ht->table);

  pmath_mem_free(ht);
}

/*============================================================================*/

PMATH_API void pmath_ht_clear(pmath_hashtable_t ht) {
  unsigned int i;

  if(!ht)
    return;

  assert(ht->klass != NULL);

  for(i = 0; i < ht->capacity; ++i)
    if(IS_USED_ENTRY(ht->table[i]))
      ht->klass->entry_destructor(ht->table[i]);

  if(ht->table != ht->small_table)
    pmath_mem_free(ht->table);

  ht->nonnull_count = 0;
  ht->used_count    = 0;
  ht->capacity      = HT_MINSIZE;
  ht->table         = ht->small_table;
  memset(ht->table, 0, ht->capacity * sizeof(void*));
}

/*============================================================================*/

PMATH_API unsigned int pmath_ht_capacity(pmath_hashtable_t ht) {
  if(ht)
    return ht->capacity;

  return 0;
}

PMATH_API unsigned int pmath_ht_count(pmath_hashtable_t ht) {
  if(ht)
    return ht->used_count;

  return 0;
}

PMATH_API void *pmath_ht_entry(pmath_hashtable_t ht, unsigned int i) {
  if(!ht || i >= ht->capacity || !IS_USED_ENTRY(ht->table[i]))
    return NULL;

  return ht->table[i];
}

/*============================================================================*/

PMATH_API
void *pmath_ht_search(pmath_hashtable_t ht, void *key) {
  void *e;

  if(!ht)
    return NULL;

  assert(ht->klass != NULL);

  e = ht->table[lookup(
                  ht,
                  key,
                  ht->klass->key_hash,
                  ht->klass->entry_equals_key)];

  return IS_USED_ENTRY(e) ? e : NULL;
}

/*============================================================================*/

PMATH_API
void *pmath_ht_remove(pmath_hashtable_t ht, void *key) {
  unsigned int index;

  if(!ht)
    return NULL;

  assert(ht->klass != NULL);

  index = lookup(ht, key, ht->klass->key_hash, ht->klass->entry_equals_key);

  if(IS_USED_ENTRY(ht->table[index])) {
    void *old = ht->table[index];

    ht->used_count--;
    ht->table[index] = DELETED_ENTRY;

    return old;
  }

  return NULL;
}

/*============================================================================*/

// You must free the result
PMATH_API void *pmath_ht_insert(pmath_hashtable_t ht, void *entry) {
  unsigned int i;

  if(!ht || !entry)
    return entry;

  assert(ht->klass != NULL);

  for(;;) {
    i = lookup(ht, entry, ht->klass->entry_hash, ht->klass->entry_keys_equal);

    if(IS_USED_ENTRY(ht->table[i])) {
      void *old = ht->table[i];
      ht->table[i] = entry;

      return old;
    }

    if((ht->nonnull_count + 1) * 3 < ht->capacity * 2) {
      if(!ht->table[i])
        ht->nonnull_count++;
      ht->used_count++;
      ht->table[i] = entry;

      return NULL;
    }

    if(!resize(ht, 2 * ht->nonnull_count)) /* 2*used_count enough? */
      return entry;
  }
}

/*============================================================================*/

PMATH_PRIVATE 
void pmath_ht_obj_class__entry_destructor(void *e) {
  struct _pmath_object_entry_t *entry = (struct _pmath_object_entry_t*)e;

  pmath_unref(entry->value);
  pmath_unref(entry->key);
  pmath_mem_free(entry);
}

PMATH_PRIVATE
unsigned int pmath_ht_obj_class__entry_hash(void *e) {
  struct _pmath_object_entry_t *entry = (struct _pmath_object_entry_t*)e;

  return pmath_hash(entry->key);
}

PMATH_PRIVATE
pmath_bool_t pmath_ht_obj_class__entry_keys_equal(void *e1, void *e2) {
  struct _pmath_object_entry_t *entry1 = (struct _pmath_object_entry_t*)e1;
  struct _pmath_object_entry_t *entry2 = (struct _pmath_object_entry_t*)e2;

  return pmath_equals(entry1->key, entry2->key);
}

PMATH_PRIVATE
unsigned int pmath_ht_obj_class__key_hash(void *k) {
  return pmath_hash(*(pmath_t*)k);
}

PMATH_PRIVATE
pmath_bool_t pmath_ht_obj_class__entry_equals_key(void *e, void *k) {
  struct _pmath_object_entry_t *entry = (struct _pmath_object_entry_t*)e;
  pmath_t                       key   = *(pmath_t*)k;
  return pmath_equals(entry->key, key);
}

PMATH_PRIVATE
const pmath_ht_class_t pmath_ht_obj_class = {
  pmath_ht_obj_class__entry_destructor,
  pmath_ht_obj_class__entry_hash,
  pmath_ht_obj_class__entry_keys_equal,
  pmath_ht_obj_class__key_hash,
  pmath_ht_obj_class__entry_equals_key
};

PMATH_PRIVATE
void *_pmath_object_entry_copy_func(void *entry) {
  struct _pmath_object_entry_t *result =
    pmath_mem_alloc(sizeof(struct _pmath_object_entry_t));

  if(result) {
    result->key   = pmath_ref(((struct _pmath_object_entry_t*)entry)->key);
    result->value = pmath_ref(((struct _pmath_object_entry_t*)entry)->value);
  }

  return result;
}

/*============================================================================*/

static void object_int_entry_destructor(void *e) {
  struct _pmath_object_int_entry_t *entry = e;
  pmath_unref(entry->key);
  pmath_mem_free(entry);
}

PMATH_PRIVATE
const pmath_ht_class_t pmath_ht_obj_int_class = {
  object_int_entry_destructor,
  pmath_ht_obj_class__entry_hash,
  pmath_ht_obj_class__entry_keys_equal,
  pmath_ht_obj_class__key_hash,
  pmath_ht_obj_class__entry_equals_key
};

PMATH_PRIVATE
void *_pmath_object_int_entry_copy_func(void *entry) { // for use with pmath_ht_copy()
  struct _pmath_object_int_entry_t *result =
    pmath_mem_alloc(sizeof(struct _pmath_object_int_entry_t));

  if(result) {
    result->key   = pmath_ref(((struct _pmath_object_int_entry_t*)entry)->key);
    result->value =           ((struct _pmath_object_int_entry_t*)entry)->value;
  }

  return result;
}

/*============================================================================*/

PMATH_PRIVATE unsigned int _pmath_hash_pointer(void *ptr) {
#if (PMATH_BITSIZE == 64) && (UINT_MAX == 0xffffffffu)
  uint64_t a = (uint64_t)ptr;
  return (unsigned int)a ^ (unsigned int)(a >> 32);
#else
  return (unsigned int)ptr;
#endif
//#if PMATH_BITSIZE == 32
//  uint32_t a = (uint32_t)ptr;
//  a = (a+0x7ed55d16) + (a<<12);
//  a = (a^0xc761c23c) ^ (a>>19);
//  a = (a+0x165667b1) + (a<<5);
//  a = (a+0xd3a2646c) ^ (a<<9);
//  a = (a+0xfd7046c5) + (a<<3);
//  a = (a^0xb55a4f09) ^ (a>>16);
//  return (unsigned int)a;
//#elif PMATH_BITSIZE == 64
////  #if PMATH_INT_32BIT
//    uint64_t a = (uint64_t)ptr;
//    a = (~a) + (a << 18); // a = (a << 18) - a - 1;
//    a = a ^ (a >> 31);
//    a = a * 21;           // a = (a + (a << 2)) + (a << 4);
//    a = a ^ (a >> 11);
//    a = a + (a << 6);
//    a = a ^ (a >> 22);
//    return (unsigned int)a;
////  #elif PMATH_INT_64BIT
////    uint64_t a = (uint64_t)ptr;
////    a = (~a) + (a << 21); // a = (a << 21) - a - 1;
////    a = a ^ (a >> 24);
////    a = (a + (a << 3)) + (a << 8); // a * 265
////    a = a ^ (a >> 14);
////    a = (a + (a << 2)) + (a << 4); // a * 21
////    a = a ^ (a >> 28);
////    a = a + (a << 31);
////    return (unsigned int)a;
////  #endif
//#endif
}

/*============================================================================*/

#ifdef PMATH_DEBUG_TESTS
typedef struct {
  int key;
  int value;
} test_entry_t;

static void test_entry_destructor(void *entry) {
}

static unsigned int test_entry_hash(void *entry) {
  return (unsigned int)((test_entry_t*)entry)->key; // bad hash!
}

static pmath_bool_t test_entry_keys_equal(void *entry1, void *entry2) {
  return ((test_entry_t*)entry1)->key ==
         ((test_entry_t*)entry2)->key;
}

static unsigned int test_key_hash(void *key) {
  return (unsigned int)(int)(size_t)key;
}

static pmath_bool_t test_entry_equals_key(void *entry, void *key) {
  return ((test_entry_t*)entry)->key == (int)(size_t)key;
}

PMATH_PRIVATE
void PMATH_TEST_NEW_HASHTABLES(void) {
  const pmath_ht_class_t klass = {
    test_entry_destructor,
    test_entry_hash,
    test_entry_keys_equal,
    test_key_hash,
    test_entry_equals_key
  };

  pmath_hashtable_t hashtable = pmath_ht_create(&klass, 0);

  if(hashtable) {
    test_entry_t a, b, c, d, *p;

    a.key = 1;
    a.value = 11;
    p = pmath_ht_insert(hashtable, &a);
    assert(p == NULL);

    b.key = 2;
    b.value = 22;
    p = pmath_ht_insert(hashtable, &b);
    assert(p == NULL);

    c.key = 3;
    c.value = 33;
    p = pmath_ht_insert(hashtable, &c);
    assert(p == NULL);

    d.key = 2;
    d.value = 2222;
    p = pmath_ht_insert(hashtable, &d);
    assert(p != NULL && p->value == 22);

    p = pmath_ht_search(hashtable, (void*)3);
    assert(p != NULL && p->value == 33);

    p = pmath_ht_remove(hashtable, (void*)7);
    assert(p == NULL);

    p = pmath_ht_remove(hashtable, (void*)1);
    assert(p != NULL && p->value == 11);

    p = pmath_ht_insert(hashtable, &a);
    assert(p == NULL);

    pmath_ht_destroy(hashtable);
  }
}
#endif
