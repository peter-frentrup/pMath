#ifndef __PMATH_TYPES_H__
#define __PMATH_TYPES_H__

#include <stdint.h>

/**\defgroup general_types General Purpose Types
   \brief Useful type definitions that do not fit into any other category.
   
  @{
 */
 
/**\brief A boolean type. 
   
   C does not define a boolean type, so we do it here for code clarity. The
   constants TRUE and FALSE can be used as return values for pmath_bool_t,
   but do not test on these. E.g. use <tt>if(test)...</tt> instead of 
   <tt>if(test == TRUE)...</tt> but <tt>return TRUE;</tt> instead of 
   <tt>return 1;</tt>
 */
typedef char pmath_bool_t;

/**\def FALSE
   \brief The FALSE value for pmath_bool_t.
 */
#ifndef FALSE
  #define FALSE ((pmath_bool_t)0)
#endif

/**\def TRUE
   \brief The TRUE value for pmath_bool_t.
 */
#ifndef TRUE
  #define TRUE (!FALSE)
#endif

#define PMATH_INVALID_PTR ((void*)UINTPTR_MAX) /* 0xffffffff... */

/**\brief A general callback function.
   
   This is used in various places where a callback function is needed, that does
   not only work with pMath objects.
 */
typedef void (*pmath_callback_t)(void*);

/** @} */

#endif /* __PMATH_TYPES_H__ */
