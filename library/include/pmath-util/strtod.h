#ifndef __PMATH_UTIL__STRTOD_H__
#define __PMATH_UTIL__STRTOD_H__

#include <pmath-config.h>

/**\brief locale-neutral strtod
 */
PMATH_API double pmath_strtod(const char *str, const char **end);

#endif // __PMATH_UTIL__STRTOD_H__
