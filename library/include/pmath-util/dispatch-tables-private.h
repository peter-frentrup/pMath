#ifndef PMATH_UTIL__DISPATCH_TABLES_PRIVATE_H__INCLUDED
#define PMATH_UTIL__DISPATCH_TABLES_PRIVATE_H__INCLUDED

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects-private.h>

#include <pmath-util/dispatch-tables.h>
#include <pmath-util/hash/hashtables.h>


/**\addtogroup dispatch_tables Dispatch tables
  @{
 */

typedef struct _pmath_custom_expr_t  _pmath_dispatch_table_expr_t;

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
  _pmath_dispatch_table_expr_t *table, // won't be freed
  pmath_t key,                        // won't be freed
  pmath_t *rules_in_rhs_out,          // will be freed, optional
  pmath_bool_t literal);

PMATH_PRIVATE void _pmath_dispatch_table_filter_limbo(
  pmath_bool_t (*keep_callback)(_pmath_dispatch_table_expr_t*, void*),
  void          *closure);
PMATH_PRIVATE void _pmath_dispatch_tables_memory_panic(void);
PMATH_PRIVATE pmath_bool_t _pmath_dispatch_tables_init(void);
PMATH_PRIVATE void _pmath_dispatch_tables_done(void);

/** @} */

#endif // PMATH_UTIL__DISPATCH_TABLES_PRIVATE_H__INCLUDED
