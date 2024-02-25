#ifndef __PMATH_UTIL__COMPRESSION_H__
#define __PMATH_UTIL__COMPRESSION_H__

#include <pmath-util/files/abstract-file.h>

/**\addtogroup file_api
  @{
 */

/**\brief Compression options.
 */
struct pmath_compressor_settings_t {
  size_t size; ///< must be sizeof(struct pmath_compressor_settings_t)
  
  /**\brief The base two logarithm of the window size.
  
     This can be between 8..15. A value of 0 gives the default. Current default is 15.
   */
  int window_bits;
  
  /**\brief Whether to wite zlib header and checksum footer.
     
     If this is TRUE, the compressor doeas a raw deflate.
   */
  pmath_bool_t skip_header;
  
  /**\brief The compression level.
  
     Can be -1 (default) or 0 (no compression) between 1 (fastest) and 9 (best), including.
   */
  int level;
  
  /**\brief The deflate strategy.
     
     The default strategy is 0. See zlib documentation for other possible values.
   */
  int strategy;
};
 
/**\brief Decompression options.
 */
struct pmath_decompressor_settings_t {
  size_t size; ///< must be sizeof(struct pmath_decompressor_settings_t)
  
  /**\brief The base two logarithm of the window size.
  
     This can be between 8..15. A value of 0 gives the default. Current default is 15.
     It must be at least as large as the windowBits used to compress the data.
   */
  int window_bits;
  
  /**\brief Whether to the stream contains zlib header and checksum footer.
     
     If this is TRUE, the compressor doeas a raw inflate.
   */
  pmath_bool_t skip_header;
};
 
/**\brief Create a writeable binary file object that compresses its input.
   \param dstfile A writable binary file object to write the compressed data to. 
                  It will be freed.
   \param options Optional settings. Can be NULL.
   \return A writeable binary file object or PMATH_NULL on error.
   
   The compression uses zlib.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_file_create_compressor(pmath_t dstfile, struct pmath_compressor_settings_t *options);

/**\brief Create a readable binary file object that decompresses its input.
   \param srcfile A readable binary file object to read the compressed data 
                  from. It will be freed.
   \param options Optional settings. Can be NULL.
   \return A readable binary file object or PMATH_NULL on error.
   
   The decompression uses zlib.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_file_create_decompressor(pmath_t srcfile, struct pmath_decompressor_settings_t *options);


/**\brief Compress an expression to a string.
   \param obj     An expression. It will be freed.
   \return A string that can be decompressed with pmath_decompress_from_string() or PMATH_NULL on error.
   
   The compression uses pmath_file_create_compressor() and encodes the binary streams as "base85", 
   prefixing with version specification "1:".
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_compress_to_string(pmath_t obj);


/**\brief Decompress an expression from a string.
   \param str     A string representing a compressed expression. It will be freed.
   \return The decompressed expression or PMATH_UNDEFINED on failure.
   
   \see pmath_compress_to_string()
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_decompress_from_string(pmath_string_t str);

/** @} */

#endif /* __PMATH_UTIL__COMPRESSION_H__ */
