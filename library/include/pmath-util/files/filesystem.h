#ifndef PMATH_UTIL_FILES_FILESYSTEM_H_INCLUDED
#define PMATH_UTIL_FILES_FILESYSTEM_H_INCLUDED

#include <pmath-core/strings.h>

/** \brief Convert an absolute or relative file path to an absolute file path if possible.
    \param relname The relative or absolute path. It will be freed.
    \return An absolute file path or PMATH_NULL. You must free it.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_to_absolute_file_name(pmath_string_t relname);

#endif // PMATH_UTIL_FILES_FILESYSTEM_H_INCLUDED
