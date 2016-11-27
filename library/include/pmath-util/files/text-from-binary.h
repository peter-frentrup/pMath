#ifndef __PMATH_UTIL__FILES__TEXT_FROM_BINARY_FILE_H__
#define __PMATH_UTIL__FILES__TEXT_FROM_BINARY_FILE_H__

#include <pmath-util/files/abstract-file.h>

/**\addtogroup file_api
  @{
 */

/**\brief Create a text file object operating on a binary file.
   \param binfile A binary file. It will be freed.
   \param encoding A text encoding that the iconv library knows.
   \return A newly created text file object. You can destroy and close it with
           pmath_file_close() or pmath_unref().
   
   If \a binfile is readable, the resulting textfile will be readable.
   If \a binfile is writable, the resulting textfile will be writable.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(2)
pmath_symbol_t pmath_file_create_text_from_binary(
  pmath_t      binfile,
  const char  *encoding);

/** @} */

#endif // __PMATH_UTIL__FILES__TEXT_FROM_BINARY_FILE_H__
