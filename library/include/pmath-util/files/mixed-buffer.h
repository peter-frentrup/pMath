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
   
   If only a binary buffer is needed, pmath_file_create_binary_buffer() should be used.
   
   If only a text buffer is needed, choose \a encoding = "latin1" and just discard 
   the returned binary buffer (with \c pmath_unref(*out_binfile) ).
 */
PMATH_API
PMATH_ATTRIBUTE_NONNULL(1)
void pmath_file_create_mixed_buffer(
  const char     *encoding,
  pmath_symbol_t *out_textfile,
  pmath_symbol_t *out_binfile);

/**\brief Set the internal text-buffer of a mixed text/binary file.
   \param txtfile The \c *out_textfile result of a call to pmath_file_create_mixed_buffer().
   \param new_text_buffer The new text buffer to use. It will be freed.
   \return The previous text buffer.
   
   This function generally only makes sense before any stream reading functions are used
   or after all stream writing functions are used.
   
   If no read or write access to the mixed buffer stream happend yet, this function
   is essentially equivalent to (but more efficient than)
   \code
int             len = pmath_string_length(new_text_buffer);
const uint16_t *buf = pmath_string_buffer(new_text_buffer); 
pmath_file_writetext(txtfile, buf, len);
   \endcode
   
   This function sets the whole in-memory stream's backing store, unlike 
   pmath_file_set_textbuffer(), which only manipulates the volatile single-line buffer.
 */
PMATH_API
pmath_string_t pmath_file_mixed_buffer_swap_text(
  pmath_symbol_t txtfile,
  pmath_string_t new_text_buffer);

/** @} */

#endif // __PMATH_UTIL__FILES__MIXED_BUFFER_H__
