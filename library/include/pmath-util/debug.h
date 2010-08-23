#ifndef __PMATH_DEBUG_H__
#define __PMATH_DEBUG_H__

#include <pmath-core/objects-inline.h>

/**\defgroup debug Debugging
   
   These functions are for logging purposes. They default to ((void)0) unless
   \c PMATH_DEBUG_LOG is defined.
   
   @{
 */

/**\brief Print out a simple debug message.
   \param fmt A printf-compatible format string
   \param ...  The variables to be printed as specified by format.
   
   The format string and arguments are as in printf.
 */
PMATH_API 
#if PMATH_NEED_GNUC(2, 4)
  __attribute__((__format__(__printf__, 1, 2)))
#endif
void pmath_debug_print(const char *fmt, ...);

/**\brief Print a pMath object to the debug log.
   \param pre A string that should be printed before the object.
   \param obj A pMath object. It wont be freed.
   \param post A string that should be printed after the object.
 */
PMATH_API 
void pmath_debug_print_object(
  const char *pre,
  pmath_t     obj, 
  const char *post);
  
#ifndef PMATH_DEBUG_LOG
  #define pmath_debug_print(...)                    ((void)0)
  #define pmath_debug_print_object(PRE, OBJ, POST)  ((void)0)
#endif /* PMATH_DEBUG_LOG */

/*@}*/

#ifdef BUILDING_PMATH
  PMATH_PRIVATE pmath_bool_t _pmath_debug_library_init(void);
  PMATH_PRIVATE void         _pmath_debug_library_done(void);
#endif
  
#endif /* __PMATH_DEBUG_H__ */
