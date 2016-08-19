#ifndef __PMATH_UTIL__FILES__BINARY_BUFFER_H__
#define __PMATH_UTIL__FILES__BINARY_BUFFER_H__

#include <pmath-util/files/abstract-file.h>

/**\addtogroup file_api
  @{
 */

/**\brief Create a byte-stream file object.
   \param mincapacity The initial size of the buffer.
   \return A newly created binary file object. You can destroy and close it with
           pmath_file_close() or pmath_unref().

   You can write to a byte-buffer and read previously written data from it. Note
   that this does not support random access to the data.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_symbol_t pmath_file_create_binary_buffer(size_t mincapacity);

/**\brief Get The number of readable bytes in a binary buffer.
   \param binfile A binary file created with pmath_file_create_binary_buffer().
          It wont be freed.
   \return The number of readable bytes in the binary buffer or 0 on error.
 */
PMATH_API
size_t pmath_file_binary_buffer_size(pmath_t binfile);

/**\brief Manipulate the content of a binary buffer.
   \param binfile A binary file created with pmath_file_create_binary_buffer().
          It wont be freed.
   \param callback The callback function that does the manipulation.
   \param closure The fourth parameter for \a callback.

   The \a callback function must not write before \a readable or after \a end.
   \a *writable gives the current write-position, which is always between
   \a readable and \a end. It can be changed inside the callback, but must remain
   between \a readable and \a end.
 */
PMATH_API
void pmath_file_binary_buffer_manipulate(
  pmath_t   binfile,
  void    (*callback)(uint8_t *readable, uint8_t **writable, const uint8_t *end, void *closure),
  void     *closure);

/** @} */

#endif // __PMATH_UTIL__FILES__BINARY_BUFFER_H__
