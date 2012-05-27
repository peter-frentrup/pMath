#ifndef __PMATH_UTIL__LINE_WRITER_H__
#define __PMATH_UTIL__LINE_WRITER_H__

#include <pmath-core/objects-inline.h>

/**\addtogroup objects
  @{
 */

/**\brief Write an object to a stream with a maximum line width.
   \memberof pmath_t
   \param obj               The object to be written.
   \param options           Some options defining the format.
   \param write             The stream's output function.
   \param user              The user-argument of write (e.g. the stream itself).
   \param page_width        The page width. This should be at least 6.
   \param indentation_width The minimum number of spaces to insert after every 
                            implicit line break.

   If \a page_width < 0, the global variable $PageWidth is used.
   Line breaks will generally not appear within single tokens (e.g. very long 
   symbol names) when those appear inside \c InputForm or when \a options 
   contains <tt>PMATH_WRITE_OPTIONS_INPUTEXPR</tt>.

   \see pmath_write
 */
PMATH_API
PMATH_ATTRIBUTE_NONNULL(3)
void pmath_write_with_pagewidth(
  pmath_t                 obj,
  pmath_write_options_t   options,
  void                  (*write)(void *user, const uint16_t *data, int len),
  void                   *user,
  int                     page_width,
  int                     indentation_width);

/** @} */

#endif // __PMATH_UTIL__LINE_WRITER_H__
