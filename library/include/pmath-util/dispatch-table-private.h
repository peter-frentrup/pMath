#ifndef PMATH_UTIL__DISPATCH_TABLE_PRIVATE_H__INCLUDED
#define PMATH_UTIL__DISPATCH_TABLE_PRIVATE_H__INCLUDED

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif


#include <pmath-core/objects-private.h>

#include <pmath-util/hashtables.h>


/**\defgroup dispatch_tables Dispatch tables
   \brief Fast lookup for lists of rules.
  
  A dispatch table is an internal pMath object attached on-demand as metadata to a 
  list of rules expression to allow fast lookup of literal rule keys.
  
  @{
 */

typedef pmath_t pmath_dispatch_table_t;

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
    
    \see _pmath_is_list_of_rules
 */
PMATH_PRIVATE pmath_dispatch_table_t _pmath_rules_need_dispatch_table(pmath_t expr);

/** Look-up a key in a list-of-rules.
    
    \param rules  A pMath object. It won't be freed.
    \param key    A pMath object. It will be freed.
    \param result A pointer to a pMath object. On success, the previous pointed-to object will 
                  be freed and replaced by the first matching rule's right-hand side.
    \return If \a rules is not a list-of-rules or if no left-hand side matches, FALSE will be returned, otherwise TRUE.
    
    On success, *result will be set to Replace(key, rules).
 */
PMATH_PRIVATE pmath_bool_t _pmath_rules_lookup(pmath_t rules, pmath_t key, pmath_t *result);

/** Find the first rule in a list-of-rules given its left-hand side.
    
    \param rules    A pMath object. It won't be freed.
    \param lhs      A pMath object. It won't be freed.
    \param literal  Whether to only consider rules whose left-hand side is exactly \a key,
                    instead of using pattern matching.
    \return The rule on success or PMATH_NULL.
 */
PMATH_PRIVATE pmath_t _pmath_rules_find_rule(pmath_t rules, pmath_t lhs, pmath_bool_t literal);

/** Change a rule's right-hand side in a list-of-rules.

    \param rules The old list-of-rules. It will be freed.
    \param key The left-hand side of the rule to change. It will be freed.
    \param callback A callback function whose first argument receives the old right-hand side 
                    (or PMATH_UNDEFINED), modifies it to the new right-hand side (or PMATH_UNDEFINED)
                    and returns whether the new rule should be Rule (TRUE) or RuleDelayed (FALSE).
                    Its second argument is TRUE if the rule had head Rule before and FALSE otherwise (i.e. for RuleDelayed)
    \param callback_context The third argument for \a callback.
    \return The new list-of-rules.
 */
PMATH_PRIVATE pmath_t _pmath_rules_modify(
  pmath_t rules, 
  pmath_t key, 
  pmath_bool_t (*callback)(pmath_t*, pmath_bool_t, void*), 
  void *callback_context);

PMATH_PRIVATE pmath_bool_t _pmath_dispatch_tables_init(void);
PMATH_PRIVATE void _pmath_dispatch_tables_done(void);

/** @} */

#endif // PMATH_UTIL__DISPATCH_TABLE_PRIVATE_H__INCLUDED
