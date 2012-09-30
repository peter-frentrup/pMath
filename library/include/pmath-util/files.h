#ifndef __PMATH_UTIL__FILES_H__
#define __PMATH_UTIL__FILES_H__

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
   \see pmath_file_status()
 */
typedef enum {
  PMATH_FILE_OK = 0,         ///< No error
  PMATH_FILE_INVALID = 1,    ///< The object is no readable file
  PMATH_FILE_ENDOFFILE = 2,  ///< The (readable) file position is at the end
  PMATH_FILE_OTHERERROR = 3, ///< There is another problem with the file.
  PMATH_FILE_RECURSIVE = 4   ///< The file is already locked by the current thread.
} pmath_files_status_t;

enum _pmath_file_properties_t {
  PMATH_FILE_PROP_READ  = 0x01,
  PMATH_FILE_PROP_WRITE = 0x02,
  
  PMATH_FILE_PROP_BINARY = 0x04,
  PMATH_FILE_PROP_TEXT   = 0x08
};

/**\brief Check whether a file supports a set of properties.
   \param file A file object. It wont be freed.
   \param properties File properties. See remarks section.
   \return TRUE iff the file supports all of the requested properties.

   \remarks
    \a properties can be zero or more of the following values:
      <ul>
        <li> \c PMATH_FILE_PROP_READ: The file is readable.

        <li> \c PMATH_FILE_PROP_WRITE: The file is writeable.

        <li> \c PMATH_FILE_PROP_BINARY: It is a binary file.

        <li> \c PMATH_FILE_PROP_TEXT: It is a text file.
      </ul>

   Lists of writeable files are writeable, too. Only Symbols can be readable
   files.
 */
PMATH_API pmath_bool_t pmath_file_test(
  pmath_t file,
  int     properties);

/**\brief Get the current status of a readable file.
   \param file A readable file object. It wont be freed.
   \return pmath_files_status_t The current file status.
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
     This can be PMATH_NULL if read_function is also PMATH_NULL.
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
  void                     *extra,
  void                    (*extra_destructor)(void*),
  pmath_binary_file_api_t  *api);

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
  
  /**\brief An optional callback function for retrieving the current stream 
     position.
   */
  int64_t (*get_pos_function)(void *extra);
  
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
  void                   *extra,
  void                  (*extra_destructor)(void*),
  pmath_text_file_api_t  *api);

/**\brief Create a text file object operating on a binary file.
   \param binfile A binary file. It will be freed.
   \param encoding A text encoding that the iconv library knows.
   \return A newly created text file object. You can destroy and close it with
           pmath_file_close() or pmath_unref().
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(2)
pmath_symbol_t pmath_file_create_text_from_binary(
  pmath_t      binfile,
  const char  *encoding);

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
   \a readable and \a end. It can be changed insidethe callback, but must remain
   between \a readable and \a end.
 */
PMATH_API
void pmath_file_binary_buffer_manipulate(
  pmath_t   binfile,
  void    (*callback)(uint8_t *readable, uint8_t **writable, const uint8_t *end, void *closure),
  void     *closure);

/** @} */

#endif /* __PMATH_UTIL__FILES_H__ */
