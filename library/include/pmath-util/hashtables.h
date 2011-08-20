#ifndef __PMATH_UTIL__HASHTABLES_H__
#define __PMATH_UTIL__HASHTABLES_H__

#include <pmath-core/objects-inline.h>

/**\defgroup hashtables Hashtables
   \brief A general hashtable implementation.

   The user of this API is responsible for the memory layout of the entries.

  @{
 */


/**\class pmath_hashtable_t
   \brief The Hashtable class
 */
typedef struct _pmath_hashtable_t *pmath_hashtable_t; // not threadsafe


/**\brief A callback function for hashtable entries
   \param entry  An entry. Never PMATH_NULL.
   \param data   Additional data for the callback.
 */
typedef void (*pmath_ht_entry_callback_t)(void *entry, void *data);


/**\brief An entry copy function
   \param entry  An entry. Never PMATH_NULL.
   \return A new entry that is a copy.
 */
typedef void *(*pmath_ht_entry_copy_t)(void *entry);


/**\brief An entry hash function
   \param entry  An entry. Never PMATH_NULL.
   \return A hash value for the entry's key.
 */
typedef unsigned int (*pmath_ht_entry_hash_func_t)(void *entry);


/**\brief A key hash function
   \param key  A key.
   \return A hash value for the key.
 */
typedef unsigned int (*pmath_ht_key_hash_func_t)(void *key);


/**\brief An entry comparision function
   \param entry1  The first entry.
   \param entry2  The second entry.
   \return TRUE if both enties' keys are equal, FALSE otherwise.
 */
typedef pmath_bool_t (*pmath_ht_entry_equal_func_t)(
  void *entry1,
  void *entry2);


/**\brief An entry to key comparision function
   \param entry  An entry.
   \param key    The key of another entry.
   \return TRUE if both entry's key equals the given key, FALSE otherwise.
 */
typedef pmath_bool_t (*pmath_ht_entry_equals_key_func_t)(
  void *entry,
  void *key);


/**\class pmath_ht_class_t
   \brief A hashtable interface.
 */
typedef struct {
  pmath_callback_t                  entry_destructor;
  pmath_ht_entry_hash_func_t        entry_hash;
  pmath_ht_entry_equal_func_t       entry_keys_equal;
  pmath_ht_key_hash_func_t          key_hash;
  pmath_ht_entry_equals_key_func_t  entry_equals_key;
} pmath_ht_class_t;

/*============================================================================*/

/**\brief Create a new hashtable.
   \memberof pmath_hashtable_t
   \param klass    An interface pointer.
   \param minsize  Initiol minimal size.
   \return The new hashtable or PMATH_NULL on error.
 */
PMATH_API pmath_hashtable_t pmath_ht_create(
  const pmath_ht_class_t  *klass,
  unsigned int             minsize);


/**\brief Copy a given hashtable.
   \memberof pmath_hashtable_t
   \param ht          The old hashtable
   \param entry_copy  A function for copying entires.
   \return The new hashtable or PMATH_NULL on error.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_hashtable_t pmath_ht_copy(
  pmath_hashtable_t      ht,
  pmath_ht_entry_copy_t  entry_copy);


/**\brief Destroy a given hashtable.
   \memberof pmath_hashtable_t
   \param ht    The hashtable.
 */
PMATH_API void pmath_ht_destroy(pmath_hashtable_t ht);


/**\brief Clear a given hashtable.
   \memberof pmath_hashtable_t
   \param ht    The hashtable.
 */
PMATH_API void pmath_ht_clear(pmath_hashtable_t ht);


/**\brief Get the capacity of a given hashtable.
   \memberof pmath_hashtable_t
   \param ht    The hashtable.
   \return The current maximum possible index of an entry in the table.
 */
PMATH_API unsigned int pmath_ht_capacity(pmath_hashtable_t ht);


/**\brief Get the number of valid entries in a given hashtable.
   \memberof pmath_hashtable_t
   \param ht    The hashtable.
   \return The number of non-PMATH_NULL entires.
 */
PMATH_API unsigned int pmath_ht_count(pmath_hashtable_t ht);


/**\brief Get any entry of a given hashtable.
   \memberof pmath_hashtable_t
   \param ht    The hashtable.
   \param i     The index. from range 0..pmath_ht_capacity(ht) - 1
   \return The entry or PMATH_NULL. It is owned by the table.
 */
PMATH_API void *pmath_ht_entry(pmath_hashtable_t ht, unsigned int i);


/**\brief Search for an entry in a given hashtable.
   \memberof pmath_hashtable_t
   \param ht    The hashtable.
   \param key   The key. It will be send to the kes_hash and entry_keys_equal
                functions of the table's interface pointer.
   \return The entry or PMATH_NULL. It is owned by the table.
 */
PMATH_API
void *pmath_ht_search(pmath_hashtable_t ht, void *key);


/**\brief Insert an entry into a given hashtable
   \memberof pmath_hashtable_t
   \param ht     The hashtable.
   \param entry  The entry. It must be compatible with all functions of the
                 table's interface.
   \return PMATH_NULL or a possible old entry or the entry itself in case of an error.
           You must destroy it.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
void *pmath_ht_insert(pmath_hashtable_t ht, void *entry);


/**\brief Remove an entry from a given hashtable
   \memberof pmath_hashtable_t
   \param ht     The hashtable.
   \param key    The entry's key. It will be send to the kes_hash and
                 entry_keys_equal functions of the table's interface pointer.
   \return PMATH_NULL or the old entry. You must destroy it.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
void *pmath_ht_remove(pmath_hashtable_t ht, void *key);

/* @} */

#endif // __PMATH_UTIL__HASHTABLES_H__
