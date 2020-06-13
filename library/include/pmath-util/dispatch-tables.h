#ifndef PMATH__UTIL__DISPATCH_TABLES_H__INCLUDED
#define PMATH__UTIL__DISPATCH_TABLES_H__INCLUDED

/**\defgroup dispatch_tables Dispatch tables
   \brief Fast lookup for lists of rules.
  
  A dispatch table is an internal pMath object attached on-demand as metadata to a 
  list of rules expression to allow fast lookup of literal rule keys.
  
  @{
 */

typedef pmath_t pmath_dispatch_table_t;

/** Check whether an object is a list of rules.
 */
PMATH_API pmath_bool_t pmath_is_list_of_rules(pmath_t obj);

/** Look-up a key in a list-of-rules.
    
    \param rules  A pMath object. It won't be freed.
    \param key    A pMath object. It will be freed.
    \param result A pointer to a pMath object. On success, the previous pointed-to object will 
                  be freed and replaced by the first matching rule's right-hand side.
    \return If \a rules is not a list-of-rules or if no left-hand side matches, FALSE will be returned, otherwise TRUE.
    
    On success, *result will be set to Replace(key, rules).
    
    \see pmath_is_list_of_rules
 */
PMATH_API pmath_bool_t pmath_rules_lookup(pmath_t rules, pmath_t key, pmath_t *result);

/** @} */

#endif // PMATH__UTIL__DISPATCH_TABLES_H__INCLUDED
