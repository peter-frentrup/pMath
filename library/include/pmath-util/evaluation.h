#ifndef __PMATH_UTIL__EVALUATION_H__
#define __PMATH_UTIL__EVALUATION_H__

#include <pmath-core/expressions.h>

/**\brief Evaluate an object.
   \relates pmath_t
   \param obj Any pMath object. It will be freed. Do not use it afterwards.
   \return A new object produced by pMath's and user defined evaluation rules.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_evaluate(pmath_t obj);

/**\brief Partly evaluate an expression.
   \relates pmath_expr_t
   \param expr A pMath expression. It will be freed. Do not use it afterwards.
   \param apply_rules Whether to apply custom rules the the whole expression or 
          not.
   \return A new object produced by pMath's and user defined evaluation rules.
   
   This function prepares an expression for evaluation 
   (evaluates its items, ...). \n
   Unlike pmath_evaluate(), it does not evaluate its result. In fact,
   pmath_evaluate() is basically a loop that calls pmath_evaluate_expression()
   until the result does not change any more.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_evaluate_expression(
  pmath_expr_t  expr,
  pmath_bool_t     apply_rules);
  
#endif /* __PMATH_UTIL__EVALUATION_H__ */
