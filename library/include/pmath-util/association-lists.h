#ifndef PMATH__UTIL__ASSOCIATION_LISTS_H__INCLUDED
#define PMATH__UTIL__ASSOCIATION_LISTS_H__INCLUDED

#include <pmath-core/expressions.h>

/**\defgroup association_lists Association lists
   \brief Column-wise representation of lists of rules expressions.
  
  An assocition list is a custom expression representation for lists of rules 
  <tt>{key1 -> value1, key2 -> value2, ...}</tt> where internally all keys and
  all values are kept in separate lists. The keys are usually represented as \ref dispatch_tables
  
  \see dispatch_tables
  
  @{
 */

/**\class pmath_association_list_t
   \extends pmath_expr_t
   \brief The Association List class.
   
   When seen as a mere expression object, an association list is a list of rules
   <tt>{key1 -> value1, key2 -> value2, key3 :> value3, ...}</tt>.
 */
typedef pmath_expr_t pmath_association_list_t;


/** \brief Test whether an expression is a list of rules represented as an association list.
    \memberof pmath_association_list_t
    \param obj An arbitrary pMath object.
    \return TRUE if \a obj is a list of rules <tt>{key1 -> value1, ...}</tt> that is moreover 
            represented internally as a \ref pmath_association_list_t.
    \see pmath_is_list_of_rules
 */
PMATH_API pmath_bool_t pmath_is_association_list(pmath_t obj);


/** \brief Convert a list of rules into an association list representation.
    \memberof pmath_association_list_t
    \param rules Pointer to a pMath object. It will be changed into an association list representation if possible.
    \return Whether the conversion was successfull, i.e. pmath_is_association_list(*rules) after the call.
 */
PMATH_PRIVATE pmath_bool_t pmath_try_make_association_list(pmath_t *rules);


/** \brief Get the list of keys of an association list expression.
    \memberof pmath_association_list_t
    \param assoc An association list. It wont be freed.
    \return The list of keys or PMATH_NULL on error.
 */
PMATH_API pmath_expr_t pmath_association_list_get_keys(pmath_association_list_t assoc);


/** \brief Get the list of values of an association list expression.
    \memberof pmath_association_list_t
    \param assoc An association list. It wont be freed.
    \return The list of values or PMATH_NULL on error.
 */
PMATH_API pmath_expr_t pmath_association_list_get_values(pmath_association_list_t assoc);


/** @} */

#endif // PMATH__UTIL__ASSOCIATION_LISTS_H__INCLUDED
