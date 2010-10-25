#ifndef __PMATH_UTIL__VERSION_H__
#define __PMATH_UTIL__VERSION_H__

#include <pmath-config.h>

/**\addtogroup helpers
  
  @{
 */

/**\brief Get the date and time when pMath was compiled.
 */
PMATH_API
void pmath_version_get_datetime(
  int *year,
  int *month,
  int *day,
  int *hour,
  int *minute,
  int *second);

/* @} */

#endif // __PMATH_UTIL__VERSION_H__
