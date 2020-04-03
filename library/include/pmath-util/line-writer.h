#ifndef __PMATH_UTIL__LINE_WRITER_H__
#define __PMATH_UTIL__LINE_WRITER_H__

#include <pmath-core/objects-inline.h>

/**\addtogroup objects
  @{
 */

/**\brief Command structure for pmath_write_with_pagewidth_ex().

   This should be inistialized with
   <tt>memset(&opts, 0, sizeof(opts)); opts.size = sizeof(opts); ... </tt>
 */
struct pmath_line_writer_options_t {
  /**\brief The structure's size in bytes.
     This must be initialized with sizeof(struct pmath_write_ex_t), for version 
     control.
   */
  size_t size;
  
  /**\brief The output flags.
   */
  pmath_write_options_t flags;
  
  /**\brief The page width. This should be at least 6.
   */
  int page_width;
  
  /**\brief The minimum number of spaces to insert after every implicit line break.
   */
  int indentation_width;
  
  /**\brief Write callback.
     
     This is mandatory.
   */
  void (*write)(void *user, const uint16_t *data, int len);
  
  /**\brief First parameter of the callbacks
   */
  void *user;
  
  /**\brief Optional. Called before an object is written.
   */
  void (*pre_write)( void *user, pmath_t obj, pmath_write_options_t options);
  
  /**\brief Optional. Called after an object is written.
   */
  void (*post_write)(void *user, pmath_t obj, pmath_write_options_t options);
  
  /**\brief Optional. Called before an object is formatted to overwrite the default behavior.
     
     Return TRUE to suppress the default display and FALSE to continue with the default display for \a obj.
   */
  pmath_bool_t (*custom_formatter)(void *user, pmath_t obj, struct pmath_write_ex_t *info);
};
  
/**\brief Write an object to a stream with a maximum line width.
   \memberof pmath_t
   \param options All the acutal parameters.
   \param obj     The object to be written.

   If <tt>options->page_width < 0</tt>, the global variable $PageWidth is used.
   Line breaks will generally not appear within single tokens (e.g. very long 
   symbol names) when those appear inside \c InputForm or when <tt>options->flags</tt>
   contains <tt>PMATH_WRITE_OPTIONS_INPUTEXPR</tt>.

   \see pmath_write_ex
 */
PMATH_API
void pmath_write_with_pagewidth_ex(
  struct pmath_line_writer_options_t *options,
  pmath_t                             obj);  
  
/**\brief Write an object to a stream with a maximum line width.
   \memberof pmath_t
   \param obj               The object to be written.
   \param flags             Some flags defining the format.
   \param write             The stream's output function.
   \param user              The user-argument of write (e.g. the stream itself).
   \param page_width        The page width. This should be at least 6.
   \param indentation_width The minimum number of spaces to insert after every 
                            implicit line break.

   If \a page_width < 0, the global variable $PageWidth is used.
   Line breaks will generally not appear within single tokens (e.g. very long 
   symbol names) when those appear inside \c InputForm or when \a flags 
   contains <tt>PMATH_WRITE_OPTIONS_INPUTEXPR</tt>.

   \see pmath_write
 */
PMATH_API
PMATH_ATTRIBUTE_NONNULL(3)
void pmath_write_with_pagewidth(
  pmath_t                 obj,
  pmath_write_options_t   flags,
  void                  (*write)(void *user, const uint16_t *data, int len),
  void                   *user,
  int                     page_width,
  int                     indentation_width);

/** @} */

#endif // __PMATH_UTIL__LINE_WRITER_H__
