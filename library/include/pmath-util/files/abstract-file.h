#ifndef __PMATH_UTIL__FILES__ABSTRACT_FILE_H__
#define __PMATH_UTIL__FILES__ABSTRACT_FILE_H__


#include <pmath-core/symbols.h>

/**\defgroup file_api File API
   \brief Unified API to access file or other memory content.

   A file is a \ref symbols "Symbol" (normally with attribute
   \ref pmath_symbol_attributes_t "PMATH_SYMBOL_ATTRIBUTE_TEMPORARY") whose
   value is a special \ref custom "Custom Object".

   The output functions can operate on lists of files.

  @{
 */

/**\brief The status of a file.
   \see pmath_file_status
 */
typedef enum {
  /**\hideinitializer
     No error.
   */
  PMATH_FILE_OK = 0,
  
  /**\hideinitializer
     The object is no readable file.
   */
  PMATH_FILE_INVALID = 1,
  
  /**\hideinitializer
     The (readable) file position is at the end.
   */
  PMATH_FILE_ENDOFFILE = 2,
  
  /**\hideinitializer
     There is another problem with the file.
   */
  PMATH_FILE_OTHERERROR = 3,
  
  /**\hideinitializer
     The file is already locked by the current thread.
   */
  PMATH_FILE_RECURSIVE = 4
} pmath_files_status_t;

/**\anchor PMATH_FILE_PROP_XXX
   \see pmath_file_test
 */
enum {
  /**\hideinitializer
     The file is readable.
   */
  PMATH_FILE_PROP_READ = 0x01,
  
  /**\hideinitializer
     The file is writeable.
   */
  PMATH_FILE_PROP_WRITE = 0x02,
  
  /**\hideinitializer
     It is a binary file.
   */
  PMATH_FILE_PROP_BINARY = 0x04,
  
  /**\hideinitializer
     It is a text file.
   */
  PMATH_FILE_PROP_TEXT = 0x08
};

/**\brief Check whether a file supports a set of properties.
   \param file A file object. It wont be freed.
   \param properties File properties. One or more of the 
                     \ref PMATH_FILE_PROP_XXX values.
   \return TRUE iff the file supports all of the requested properties.

   Lists of writeable files are writeable, too. Only Symbols can be readable
   files.
 */
PMATH_API pmath_bool_t pmath_file_test(
  pmath_t file,
  int     properties);

/**\brief Get the current status of a readable file.
   \param file A readable file object. It wont be freed.
   \return The current file status.
 */
PMATH_API pmath_files_status_t pmath_file_status(pmath_t file);

/**\brief Read some bytes from a binary file
   \param file A readable binary file object. It wont be freed.
   \param buffer The read bytes will go here.
   \param buffer_size The number of bytes you would like to read/size of
          \a buffer.
   \param preserve_internal_buffer If TRUE, a subsequent call will get the same
          buffer content. If FALSE, the file pointer will be moved.
   \return Number of read bytes. This is never more than \a buffer_size.
           If \a preserve_internal_buffer is TRUE or in case of an error, this
           can be less than \a buffer_size.
 */
PMATH_API size_t pmath_file_read(
  pmath_t       file,
  void         *buffer,
  size_t        buffer_size,
  pmath_bool_t  preserve_internal_buffer);

/**\brief Read one line from a text file.
   \param file A readable text file object. It wont be freed.
   \return The next line in the file.
 */
PMATH_API pmath_string_t pmath_file_readline(pmath_t file);

/**\brief Set a file's internal text buffer.
   \param file A readable text file. It wont be freed.
   \param buffer A string. It should not contain any new line characters. It 
                 will be freed.
 */
PMATH_API
void pmath_file_set_textbuffer(pmath_t file, pmath_string_t buffer);

/**\brief Write some bytes to a binary file.
   \param file A writeable binary file object. It wont be freed.
   \param buffer The data to be written.
   \param buffer_size The number of bytes to write/size of \a buffer.
   \return The number of written bytes. This is less than buffer_size in case of
           an error. If file is a list of files this is the smallest number of
           written bytes to the single files.
 */
PMATH_API size_t pmath_file_write(
  pmath_t      file,
  const void  *buffer,
  size_t       buffer_size);

/**\brief Write to a text file.
   \param file A writeable text file object. It wont be freed.
   \param str A UTF-16 string buffer. e.g. pmath_string_buffer(&some_text)
   \param len The number of uint16_t in the buffer. e.g.
          pmath_string_length(some_text). This can be -1 if \a str is zero
          terminated.
   \return Whether the operation succeeded.
 */
PMATH_API pmath_bool_t pmath_file_writetext(
  pmath_t          file,
  const uint16_t  *str,
  int              len);

/**\brief Flush the output buffer of a writeable file.
   \param file A writeable file object (binary or text). It wont be freed.
 */
PMATH_API void pmath_file_flush(pmath_t file);

/**\brief Get the stream's position, if possible
   \param file A file object. It wont be freed.
 */
PMATH_API int64_t pmath_file_get_position(pmath_t file);

/**\brief An optional callback function for setting (seeking) the current stream 
   position. 
   
   \param file   A file object. It wont be freed.
   \param offset The distance by which the file pointer should be moved
   \param origin Whether \a offset is relative to the beginning of the file
                 (SEEK_SET), to the current position (SEEK_CUR) or the end
                 (SEEK_END).
   \return Whether the position was changed successfully.
 */
PMATH_API pmath_bool_t pmath_file_set_position(
  pmath_t file, 
  int64_t offset, 
  int     origin);
  
/**\brief Write an object to a text file.
   \param file A writeable text file object. It wont be freed.
   \param obj An object. It wont be freed.
   \param options Some options defining the format.
   \return Whether the operation succeeded.
 */
PMATH_API pmath_bool_t pmath_file_write_object(
  pmath_t                file,
  pmath_t                obj,
  pmath_write_options_t  options);

/**\brief Set a binary file's buffer size.
   \param file A binary file. It wont be freed.
   \param size The new size of the buffer in bytes.
   \return TRUE if the operation succeded.

   This function might clear the old buffer. So it should be called before any
   file read operation is done.
 */
PMATH_API
pmath_bool_t pmath_file_set_binbuffer(
  pmath_t file,
  size_t  size);

/**\brief Manipulate a file's internal representation.
   \param file A file object. It wont be freed.
   \param type The \a extra_destructor that was provided to
          pmath_file_create_binary() or pmath_file_create_text().
   \param callback A callback function. The first argument will be the \a extra
          parameter that was provided to pmath_file_create_binary() or
          pmath_file_create_text().
   \param data The second parameter for \a callback.

   If \a file is a valid file object and if \a type is the \a extra_destructor
   which \a file was created with, then and only then \a callback will be called.

   This function does not support lists of writeable files.
 */
PMATH_API
PMATH_ATTRIBUTE_NONNULL(3)
void pmath_file_manipulate(
  pmath_t   file,
  void    (*type)(void*),
  void    (*callback)(void*, void*),
  void     *data);

/**\brief Closes a file.
   \param file A file object. It will be freed.
   \return Whether file was a valid file object.
   
   Files are closed automatically by the garbage collector when the reference 
   counter becomes zero, which could be at an unpredictable point time in the 
   future. This function closes a file immediatly (by clearing the value of the 
   symbol representing the file).
 */
PMATH_API
pmath_bool_t pmath_file_close(pmath_t file);

/**\brief Closes a file if it is not referenced somewhere else.
   \param file A file object. It will be freed.
   
   \see pmath_file_close
 */
PMATH_API
void pmath_file_close_if_unused(pmath_t file);

/**\class pmath_binary_file_api_t
   \brief Access functions for binary files.

   \see pmath_file_create_binary
 */
typedef struct {
  /**\brief The structure size.
     Allways initialize this with \c sizeof(pmath_binary_file_api_t).
   */
  size_t struct_size;
  
  /**\brief A function to get the current file status
     This can be NULL if read_function is also NULL.
   */
  pmath_files_status_t (*status_function)(void *extra);
  
  /**\brief An optional callback function for reading bytes.
   */
  size_t (*read_function)(void *extra, void *buffer, size_t buffer_size);
  
  /**\brief An optional callback function for writing bytes.
   */
  size_t (*write_function)(void *extra, const void *buffer, size_t buffer_size);
  
  /**\brief An optional callback function for flushing an output buffer.
   */
  void (*flush_function)(void *extra);
  
  /**\brief An optional callback function for retrieving the current stream 
     position.
   */
  int64_t (*get_pos_function)(void *extra);
  
  /**\brief An optional callback function for setting the current stream 
     position. origin can be any of SEEK_SET (0), SEEK_CUR (1) or SEEK_END (2).
   */
  pmath_bool_t (*set_pos_function)(void *extra, int64_t offset, int origin);
  
} pmath_binary_file_api_t;

/**\brief Create a binary file object.
   \param extra Arbitrary extra data.
   \param extra_destructor A function to destroy the extra data.
   \param api The file access functions.
   \return A newly created binary file object. You can destroy and close it with
           pmath_file_close() or pmath_unref().

   The \a api functions are never called by more than one thread at once. This
   is assured with a non-reentrant spinlock.

   \see pmath_binary_file_api_t
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(2)
pmath_symbol_t pmath_file_create_binary(
  void                           *extra,
  void                          (*extra_destructor)(void*),
  const pmath_binary_file_api_t  *api);

/**\class pmath_text_file_api_t
   \brief Access functions for text files.

   \see pmath_file_create_text
 */
typedef struct {
  /**\brief The structure size.
     Allways initialize this with \c sizeof(pmath_binary_file_api_t).
   */
  size_t struct_size;
  
  /**\brief A function to get the current file status
     This can be PMATH_NULL if read_function is also PMATH_NULL.
   */
  pmath_files_status_t (*status_function)(void *extra);
  
  /**\brief An optional callback function for reading one line.
   */
  pmath_string_t (*readln_function)(void *extra);
  
  /**\brief An optional callback function for writing one line.
   */
  pmath_bool_t (*write_function)(void *extra, const uint16_t *str, int len);
  
  /**\brief An optional callback function for flushing an output buffer.
   */
  void (*flush_function)(void *extra);
  
} pmath_text_file_api_t;

/**\brief Create a text file object.
   \param extra Arbitrary extra data.
   \param extra_destructor A function to destroy the extra data.
   \param api The file access functions.
   \return A newly created text file object. You can destroy and close it with
           pmath_file_close() or pmath_unref().

   The \a api functions are never called by more than one thread at once. This
   is assured with a non-reentrant spinlock.

   \see pmath_text_file_api_t
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(2)
pmath_symbol_t pmath_file_create_text(
  void                         *extra,
  void                        (*extra_destructor)(void*),
  const pmath_text_file_api_t  *api);
  
/** @} */

#endif // __PMATH_UTIL__FILES__ABSTRACT_FILE_H__
