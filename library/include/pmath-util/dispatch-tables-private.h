#ifndef PMATH_UTIL__DISPATCH_TABLES_PRIVATE_H__INCLUDED
#define PMATH_UTIL__DISPATCH_TABLES_PRIVATE_H__INCLUDED

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects-private.h>

#include <pmath-util/dispatch-tables.h>
#include <pmath-util/hashtables.h>


/**\addtogroup dispatch_tables Dispatch tables
  @{
 */

struct _pmath_dispatch_entry_t {
  pmath_t key;
  unsigned int literal_turn_or_zero;
  /* TODO: on x64, here are 4 unused padding bytes. 
      Either uses size_t above or use an int offset instead of a pointer below.
  */
  struct _pmath_dispatch_entry_t *next_slice_or_slice_start;
};

struct _pmath_dispatch_table_t {
  struct _pmath_t inherited;
  pmath_t all_keys;
  pmath_hashtable_t literal_entries;
  struct _pmath_dispatch_entry_t entries[1]; // count is pmath_expr_length(keys)
};

/** Get the dispatch-table for a list-of-rules.
    
    \param expr A pMath object. It won't be freed.
    \return A dispatch-table if \a expr is a list-of-rules or PMATH_NULL on error.
    
    If \a expr is a list-of-rules without a previously attached dispatch-table, a new dispatch-table 
    will be attached to expr atomically (and in-place).
    
    \see pmath_is_list_of_rules
 */
PMATH_PRIVATE pmath_dispatch_table_t _pmath_rules_need_dispatch_table(pmath_t expr);

/** Find the first rule in a list-of-rules given its left-hand side.
    
    \param rules    A pMath object. It won't be freed.
    \param lhs      A pMath object. It won't be freed.
    \param literal  Whether to only consider rules whose left-hand side is exactly \a key,
                    instead of using pattern matching.
    \return The rule on success or PMATH_NULL.
 */
PMATH_PRIVATE pmath_t _pmath_rules_find_rule(pmath_t rules, pmath_t lhs, pmath_bool_t literal);

PMATH_PRIVATE void _pmath_dispatch_tables_memory_panic(void);
PMATH_PRIVATE pmath_bool_t _pmath_dispatch_tables_init(void);
PMATH_PRIVATE void _pmath_dispatch_tables_done(void);

/** @} */

#endif // PMATH_UTIL__DISPATCH_TABLES_PRIVATE_H__INCLUDED
