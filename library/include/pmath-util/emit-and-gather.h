#ifndef __PMATH_UTIL__EMIT_AND_GATHER_H__
#define __PMATH_UTIL__EMIT_AND_GATHER_H__

#include <pmath-core/expressions.h>

/**\addtogroup helpers

  @{
 */

/**\brief Start gathering emitted objects.
   \relates pmath_expr_t
   \param pattern A pattern that is used to determine which emitted objects 
          should be gathered (testing the emit-tag, not the object itself). It 
          will be freed.

   Use pmath_gather_end() to finish gathering. Calls to
   pmath_gather_begin() ... pmath_gather_end() can be nested.

   The emit-and-gather mechanism is useful when you want to create a list but do
   not know its final length in advance.

   \see pmath_emit
 */
PMATH_API 
void pmath_gather_begin(pmath_t pattern);

/**\brief Finish gathering emitted objects.
   \relates pmath_expr_t
   \return A list of all emitted objects since the last 
           pmath_gather_begin() whose emit-tag matched the \c pattern 
           parameter given to that pmath_gather_begin(). You must free 
           it.

   \see pmath_emit
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_expr_t pmath_gather_end(void);

/**\brief Emit an object to be gathered by the appropriate surounding
   pmath_gather_begin() ... pmath_gather_end() function pair.
   \relates pmath_expr_t
   \param object The objcet to be emitted, it will be freed.
   \param tag A tag object. The sourounding Gather() with a pattern, that 
          matches \c tag will collect the \c object. \c tag will be freed.

   \see pmath_gather_begin
   \see pmath_gather_end
 */
PMATH_API 
void pmath_emit(
  pmath_t object,
  pmath_t tag);

/** @} */

#endif /* __PMATH_UTIL__EMIT_AND_GATHER_H__ */
