#ifndef __PMATH_UTIL__APPROXIMATE_H__
#define __PMATH_UTIL__APPROXIMATE_H__

#include <pmath-core/objects-inline.h>

/**\addtogroup numbers

   @{
 */

/**\brief Get the accuracy (in bits) of an object.
   \param obj An object. It will be freed.
   \return The number of known bits after the decimal point.
   
   HUGE_VAL is given for exact quantities.
   If \a obj is an expression, the minimum of its items' accuracies is returned.
   
   Note that the builtin function Accuracy() uses base 10, but this function
   operates on base 2.
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
double pmath_accuracy(pmath_t obj);

/**\brief Get the precision (in bits) of an object.
   \param obj An object. It will be freed.
   \return The number of known bits.
   
   HUGE_VAL is given for exact quantities. -HUGE_VAL means "machine precision".
   If \a obj is an expression, the minimum of its items' accuracies is returned.
   
   Note that the builtin function Precision() uses base 10, but this function
   operates on base 2.
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE
double pmath_precision(pmath_t obj);

/**\brief Set an object's accuracy in bits.
   \param obj An object. It will be freed.
   \param acc The new number of known bits after the decimal point.
   \return The new object.
   
   Use <tt>acc == -HUGE_VAL</tt> for machine precision. 
   
   Note that the builtin function SetAccuracy() uses base 10, but this 
   function operates on base 2.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT 
pmath_t pmath_set_accuracy(pmath_t obj, double acc);

/**\brief Set an object's accuracy in bits.
   \param obj An object. It will be freed.
   \param prec The new number of known bits.
   \return The new object.
   
   Use <tt>prec == -HUGE_VAL</tt> for machine precision. 
   
   Note that the builtin function SetPrecision() uses base 10, but this 
   function operates on base 2.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT 
pmath_t pmath_set_precision(pmath_t obj, double prec);

/**\brief Approximate an object.
   \param obj An object. It will be freed.
   \param prec The requested precision in bits.
   \param acc The requested accurarcy in bits.
   \return The approximated object.
   
   Use <tt>prec == -HUGE_VAL</tt> or <tt>acc == -HUGE_VAL</tt> for machine 
   precision. Use <tt>acc == HUGE_VAL</tt> if the accuracy is not imporant and
   use <tt>prec == HUGE_VAL</tt> if the precision is not important.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT 
pmath_t pmath_approximate(
  pmath_t obj,  // will be freed
  double  prec, // -inf = MachinePrecision
  double  acc); // -inf = MachinePrecision

/** @} */

#endif /* __PMATH_UTIL__APPROXIMATE_H__ */
