#ifndef __PMATH_BUILTINS__LOGIC_PRIVATE_H__
#define __PMATH_BUILTINS__LOGIC_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects.h>

/* This header exports all definitions of the sources in
   src/pmath-builtins/logic-private/
 */
 

#define PMATH_DIRECTION_LESS      (1<<0)
#define PMATH_DIRECTION_EQUAL     (1<<1)
#define PMATH_DIRECTION_GREATER   (1<<2)


#define  PMATH_MAYBE_ORDERED  (-1)
#define  PMATH_UNORDERED      (-2)  // not yet used

/** Compare to values.
    \param prev       The first value. It won't be freed.
    \param next       The second value. It won't be freed.
    \param directions Which directions to test. Combination of PMATH_DIRECTION_XXX flags.
    
    \return Either TRUE or FALSE or PMATH_MAYBE_ORDERED or PMATH_UNORDERED.
 */
int _pmath_numeric_order(pmath_t prev, pmath_t next, int directions);

#endif /* __PMATH_BUILTINS__LOGIC_PRIVATE_H__ */
