#ifndef __PMATH_UTIL__VERSION_H__
#define __PMATH_UTIL__VERSION_H__

#include <pmath-config.h>

/**\addtogroup helpers
  
  @{
 */

/**\brief Get the date and time when pMath was compiled.
 */
PMATH_API
void pmath_version_datetime(
  int *year,
  int *month,
  int *day,
  int *hour,
  int *minute,
  int *second);

/**\brief Get version number (major + minor/100)
 */
PMATH_API
double pmath_version_number(void);

/**\brief Get version number part.
   \param index The number index. Major=1, Minor=2, Revision=3, Subversion=4
 */
PMATH_API
long pmath_version_number_part(int index);

/* @} */

#endif // __PMATH_UTIL__VERSION_H__
