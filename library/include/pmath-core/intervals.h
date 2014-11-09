#ifndef __PMATH_CORE__INTERVALS_H__
#define __PMATH_CORE__INTERVALS_H__

#include <pmath-core/expressions.h>

/**\defgroup intervals Intervals
   \brief Interval arithmetic with MPFI
   
   @{
 */

/**\class pmath_interval_t
   \extends pmath_t
   \brief A multi-precision floating point interval of the real axis.
 */
typedef pmath_t pmath_interval_t;

/**\brief Create an interval object from an expression.
   \param obj An expression of the form <tt>Internal`RealInterval(left, right)</tt> 
              or <tt>Internal`RealInterval(left, right, precision)</tt>.
              It will be freed.
   \return A new interval object on success, and \a obj otherwise.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_interval_from_expr(pmath_t obj);

/**\brief Convert an interval object to an expression.
   \param interval An interval object. It wont be freed.
   \return An expression of the form <tt>Internal`RealInterval(...)</tt>
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_interval_get_expr(pmath_interval_t interval);


PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_interval_get_left(pmath_interval_t interval);

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_interval_get_right(pmath_interval_t interval);

/* @} */

#endif // __PMATH_CORE__INTERVALS_H__
