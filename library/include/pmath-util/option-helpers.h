#ifndef __PMATH_UTIL__OPTION_HELPERS_H__
#define __PMATH_UTIL__OPTION_HELPERS_H__

#include <pmath-core/expressions.h>


/**\addtogroup helpers Object Utility Functions
   \brief Utility functuions for pMath Objects and Expressions.

   Here are some utility functions that simplify access to Expressions (or pMath
   Objects in general), but do not realy fit one of these topics.

  @{
 */

/**\brief Test whether an expression is a set of options.
   \relates pmath_expr_t
   \param expr An expression. It wont be freed.
   \return TRUE if Flatten({ \a expr }) is a list of rules.
   
   This is a shorthand for pmath_check_set_of_options(expr, PMATH_UNDEFINED, PMATH_NULL).
 */
PMATH_API 
pmath_bool_t pmath_is_set_of_options(pmath_t expr);

/**\brief Extract option values from an expression.
   \relates pmath_expr_t
   \param expr           The expression containing option values. It wont be 
                         freed.
   \param last_nonoption The index of the last argument that is not an option 
                         rule.
   \return A set of all given option values or PMATH_NULL on error. You must 
           free it.

   Imagine, \c expr = `F(a,b,{A->1},B->2)` and \c last_nonoption = 2, then the
   return value is a list `{{A->1}, B->2}` if both A and B are option names of 
   F. You can now use this return value as the \c extra parameter in 
   \ref pmath_option_value().

   If \c last_nonoption was 1 or if A or B where no option names of F, a message 
   would be generated (b is no rule ...) and the return value is PMATH_NULL. In 
   that case, the calling function should have no further effects and return.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_options_extract(pmath_expr_t expr, size_t last_nonoption);

/**\brief Retrieve a option value of a given function.
   \relates pmath_expr_t
   \param head The function for which the requested option value is defined. It
               wont be freed. If it is PMATH_NULL, the current head 
               (see \ref pmath_current_head ) will be used.
   \param name The name of the option value (in general, a symbol). It wont be
               freed.
   \param more A set of extra option rules or PMATH_UNDEFINED as given by 
               pmath_options_extract(). It wont be freed.
   \return The requested option value.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_option_value(pmath_t head, pmath_t name, pmath_t more);

/* @} */

#endif // __PMATH_UTIL__OPTION_HELPERS_H__
