#ifndef __PMATH_UTIL__FILES__MIXED_BUFFER_H__
#define __PMATH_UTIL__FILES__MIXED_BUFFER_H__

#include <pmath-util/files/abstract-file.h>

/**\addtogroup file_api
  @{
 */

/**\brief Creates a mixed binary/text file double ended queue.
   \param encoding The encoding name. Possible values are "latin1", and 
                   "base85"
   \param out_textfile Will be set to the readable/writable text end.
   \param out_binfile  Will be set to the readable/writable binary end.
 */
PMATH_API
PMATH_ATTRIBUTE_NONNULL(1)
void pmath_file_create_mixed_buffer(
  const char     *encoding,
  pmath_symbol_t *out_textfile,
  pmath_symbol_t *out_binfile);

/** @} */

#endif // __PMATH_UTIL__FILES__MIXED_BUFFER_H__
