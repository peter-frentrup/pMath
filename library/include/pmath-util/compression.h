#ifndef __PMATH_UTIL__COMPRESSION_H__
#define __PMATH_UTIL__COMPRESSION_H__

#include <pmath-util/files.h>

/**\addtogroup file_api
  @{
 */
 
/**\brief Create a binary file object that compresses its input.
   \param dstfile A writable binary file object to write the compressed data to. 
                  It will be freed.
   \return A binary file object or PMATH_NULL on error.
   
   The compression uses zlib.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_symbol_t pmath_file_create_compressor(pmath_t dstfile);

/**\brief Create a binary file object that uncompresses its input.
   \param srcfile A readable binary file object to read the compressed data 
                  from. It will be freed.
   \return A binary file object or PMATH_NULL on error.
   
   The uncompression uses zlib.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_symbol_t pmath_file_create_uncompressor(pmath_t srcfile);


/* @} */

#endif /* __PMATH_UTIL__COMPRESSION_H__ */
