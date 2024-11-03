#ifndef PMATH__UTIL__DISPATCH_TABLES_H__INCLUDED
#define PMATH__UTIL__DISPATCH_TABLES_H__INCLUDED

#include <pmath-core/expressions.h>

/**\defgroup dispatch_tables Dispatch tables
   \ingroup expressions
   \brief Fast lookup for lists of rules.
  
  A dispatch table is a pMath expression object that represents a list of (typically literal) 
  patterns, but has a custom internal representation to allow fast lookup.
  It is attached on-demand as metadata to a list of rules expression to allow 
  fast lookup of literal rule keys.
  
  @{
 */

/**\class pmath_dispatch_table_t
   \extends pmath_expr_t
   \brief The Dispatch Table class.
   
   When seen as a mere expression object, a dispatch table is a list of elements <tt>{e1, e2, ...}</tt>.
 */
typedef pmath_expr_t pmath_dispatch_table_t;

/** Check whether an object is a list of rules.
    \relates pmath_dispatch_table_t
    \relates pmath_association_list_t
    \param obj An arbitrary pMath object.
    \return TRUE if the object is a list of rules.
 */
PMATH_API pmath_bool_t pmath_is_list_of_rules(pmath_t obj);

/** Check whether an object is a dispatch table.
    \memberof pmath_dispatch_table_t
    \param obj An arbitrary pMath object.
    \return TRUE if the object is a list of expressions represented by a \a pmath_dispatch_table_t.
 */
PMATH_API pmath_bool_t pmath_is_dispatch_table(pmath_t obj);

/** Look-up a key in a list-of-rules.
    \relates pmath_dispatch_table_t
    \relates pmath_association_list_t
    \param rules  A pMath object. It won't be freed.
    \param key    A pMath object. It will be freed.
    \param result A pointer to a pMath object. On success, the previous pointed-to object will 
                  be freed and replaced by the first matching rule's right-hand side.
    \return If \a rules is not a list-of-rules or if no left-hand side matches, FALSE will be returned, otherwise TRUE.
    
    On success, *result will be set to Replace(key, rules).
    
    \see pmath_is_list_of_rules
 */
PMATH_API pmath_bool_t pmath_rules_lookup(pmath_t rules, pmath_t key, pmath_t *result);

/** Change a rule's right-hand side in a list-of-rules.
    \relates pmath_dispatch_table_t
    \relates pmath_association_list_t
    \param rules The old list-of-rules. It will be freed.
    \param key The left-hand side of the rule to change. It will be freed.
    \param callback A callback function whose first argument receives the old right-hand side 
                    (or PMATH_UNDEFINED), modifies it to the new right-hand side (or PMATH_UNDEFINED)
                    and returns whether the new rule should be Rule (TRUE) or RuleDelayed (FALSE).
                    Its second argument is TRUE if the rule had head Rule before and FALSE otherwise (i.e. for RuleDelayed)
    \param callback_context The third argument for \a callback.
    \return The new list-of-rules.
 */
PMATH_API pmath_t pmath_rules_modify(
  pmath_t rules, 
  pmath_t key, 
  pmath_bool_t (*callback)(pmath_t*, pmath_bool_t, void*), 
  void *callback_context);

/** @} */

#endif // PMATH__UTIL__DISPATCH_TABLES_H__INCLUDED
