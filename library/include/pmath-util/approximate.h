#ifndef __PMATH_UTIL__APPROXIMATE_H__
#define __PMATH_UTIL__APPROXIMATE_H__

#include <pmath-core/objects-inline.h>

/**\addtogroup numbers

   @{
 */

/**\brief Test whether an expression is a numeric quantity.
   \param obj An object. It wont be freed.
   \return Whether calling pmath_set_precision[_interval]() may return an approximate
           floating point number or interval.
 */
PMATH_API pmath_bool_t pmath_is_numeric(pmath_t obj);

/**\brief Get the precision (in bits) of an object.
   \param obj An object. It will be freed.
   \return The number of known bits.

   HUGE_VAL is given for exact quantities. -HUGE_VAL means "machine precision".
   If \a obj is an expression, the minimum of its items' precisions is returned.

   Note that the builtin function Precision() uses base 10, but this function
   operates on base 2.
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
double pmath_precision(pmath_t obj);

/**\brief Set an object's precision in bits.
   \param obj An object. It will be freed.
   \param prec The new number of known bits.
   \return The new object.

   Use <tt>prec == -HUGE_VAL</tt> for machine precision and
   <tt>prec == -HUGE_VAL</tt> if you want to convert all floating point numbers
   to exact rational numbers.

   Note that the builtin function SetPrecision() uses base 10, but this
   function operates on base 2.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_set_precision(pmath_t obj, double prec);

/** @} */

#endif /* __PMATH_UTIL__APPROXIMATE_H__ */
