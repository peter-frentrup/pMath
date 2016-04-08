#ifndef __PMATH_UTIL__FORMATTING_PRIVATE_H__
#define __PMATH_UTIL__FORMATTING_PRIVATE_H__


#include <pmath-core/objects.h>


/**\brief get the associated Format rule for obj.
   
   \param obj A pMath object. It won't be freed.
   \return The associated Format definition or PMATH_UNDEFINED.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_get_user_format(pmath_t obj);


PMATH_PRIVATE
pmath_bool_t _pmath_write_user_format(struct pmath_write_ex_t *info, pmath_t obj);


#endif // __PMATH_UTIL__FORMATTING_PRIVATE_H__
