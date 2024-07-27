#ifndef PMATH_UTIL__DISPATCH_TABLES_PRIVATE_H__INCLUDED
#define PMATH_UTIL__DISPATCH_TABLES_PRIVATE_H__INCLUDED

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects-private.h>

#include <pmath-util/dispatch-tables.h>
#include <pmath-util/hash/hashtables.h>


#include <pmath-core/expressions-private.h> // for struct _pmath_custom_expr_data_t . to be moved out of this header


/**\addtogroup dispatch_tables Dispatch tables
  @{
 */

struct _pmath_dispatch_entry_t {
  unsigned int literal_turn_or_zero;
  pmath_bool_t is_const_pattern_sequence;
  // ... padding :-/ ...
  struct _pmath_dispatch_entry_t *next_slice_or_slice_start;
};

struct _pmath_dispatch_table_extra_data_t {
  struct _pmath_custom_expr_data_t base;     // let capacity := expr.internals.length + 1, the number of expr.internals.items[]
 
  size_t used_length;                        // used_length <= capacity
  pmath_hashtable_t literal_entries;
  struct _pmath_dispatch_entry_t entries[1]; // capacity many elements
};

struct _pmath_dispatch_table_t {
  struct _pmath_t inherited;
  pmath_t all_keys;
  struct _pmath_dispatch_table_extra_data_t extra; // capacity:= pmath_expr_length(keys)
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

/** Look-up a key.
    
    If rules_in_rhs_out is given, it must point to (a copy of) a list of rules whose left-hand-sides 
    are the keys of \a table. It will be used for pattern matching (respecting any Condition in the 
    corresponding rhs). On output, the list-of-rules will be freed and be replaced by the matched 
    right-hand side. It then needs to be freed.
 */
PMATH_PRIVATE size_t _pmath_dispatch_table_lookup(
  struct _pmath_dispatch_table_t *table, // won't be freed
  pmath_t key,                           // won't be freed
  pmath_t *rules_in_rhs_out,             // will be freed, optional
  pmath_bool_t literal);

PMATH_PRIVATE void _pmath_dispatch_table_filter_limbo(
  pmath_bool_t (*keep_callback)(const struct _pmath_dispatch_table_t*, void*),
  void          *closure);
PMATH_PRIVATE void _pmath_dispatch_tables_memory_panic(void);
PMATH_PRIVATE pmath_bool_t _pmath_dispatch_tables_init(void);
PMATH_PRIVATE void _pmath_dispatch_tables_done(void);

/** @} */

#endif // PMATH_UTIL__DISPATCH_TABLES_PRIVATE_H__INCLUDED
