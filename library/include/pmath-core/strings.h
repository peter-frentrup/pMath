#ifndef __PMATH_CORE__STRING_H__
#define __PMATH_CORE__STRING_H__

#include <pmath-core/objects-inline.h>

/**\defgroup strings Strings
   \brief String objects in pMath.

   pMath stores strings in UCS-2 format (like Java and Windows NT). But pMath 
   strings are not zero terminated.

   Do not confuse pMath String characters (uint16_t) with wchar_t:
   sizeof(wchar_t) differs on different Systems (Linux: 4 bytes, Windows: 2
   bytes). So you cannot simply convert wchar_t* strings to pMath strings.

  @{
 */
 
/**\class pmath_string_t
   \extends pmath_t
   \brief The string class.

   Because pmath_string_t is derived from pmath_t, you can use strings
   wherever a pmath_t is accepted. E.g. you compare two strings with
   pmath_compare() or pmath_equals().

   The pmath_type_t of strings is PMATH_TYPE_STRING.

   \see objects
 */
typedef pmath_t pmath_string_t;

/**\brief Create an empty pMath String.
   \memberof pmath_string_t
   \param capacity The initial capacity of the string. Must not be negative.
   \return A new pMath String or PMATH_NULL on failure. You must destroy it.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_string_new(int capacity);

/**\brief Insert an Latin-1 encoded buffer into a pMath String.
   \memberof pmath_string_t
   \param str A pMath String or PMATH_NULL. It will be freed.
   \param inspos The position, at which \c ins should be inserted in \c str.
   \param ins A byte string.
   \param inslen The length of \c ins or -1 if it is zero-terminated.
   \return PMATH_NULL on failure or a pMath String. You must destroy it.

   If \c str is PMATH_NULL, it is assumed to be the empty string.
   The result is equivalent to a call to pmath_string_insert_codepage() with a
   codepage that translates every byte \b b to \b (uint16_t)(unsigned char)b.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_string_insert_latin1(
  pmath_string_t str,
  int            inspos,
  const char    *ins,
  int            inslen);

/**\brief Convert an UTF-8 encoded buffer to a pMath String.
   \memberof pmath_string_t
   \param str A byte string. It wont be freed.
   \param len The byte-length of \c ins or -1 if it is zero-terminated.
   \return PMATH_NULL on failure or a pMath String. You must destroy it.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_string_from_utf8(
  const char    *str,
  int            len);

/**\brief Convert a pMath string to a zero-terminated UTF-8 string.
   \memberof pmath_string_t
   \param str A pMath string. It wont be freed.
   \param result_len Position, where the string length of the returned buffer
          may be stored.
   \return A zero-terminated UTF-8  string or PMATH_NULL on error. You have to free
           the memory with pmath_mem_free(result, *size_ptr).
   
   \note pMath strings may contain embedded '\0', but C strings may not. 
         However, the conversion is done to the whole string even though your C
         functions will only \em see the content up to the first '\0'.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
char *pmath_string_to_utf8(
  pmath_string_t  str,
  int            *result_len);

/**\brief Convert a string buffer in the current console character encoding to a 
          pMath String.
   \memberof pmath_string_t
   \param str A byte string. It wont be freed.
   \param len The byte-length of \c ins or -1 if it is zero-terminated.
   \return PMATH_NULL on failure or a pMath String. You must destroy it.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_string_from_native(
  const char    *str,
  int            len);

/**\brief Convert a pMath string to a string in the current console character 
          encoding.
   \memberof pmath_string_t
   \param str A pMath string. It wont be freed.
   \param result_len Position, where the string length of the returned buffer
          may be stored.
   \return A zero terminated string or PMATH_NULL on error. You have to free the 
           memory with pmath_mem_free(result, *size_ptr).
   
   \note pMath strings may contain embedded '\0', but C strings may not. 
         However, the conversion is done to the whole string even though your C
         functions will only \em see the content up to the first '\0'.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
char *pmath_string_to_native(
  pmath_string_t  str,
  int            *result_len);

/**\brief Additional information for pmath_utf8_writer() or 
          pmath_native_writer().
 */
typedef struct {
  void (*write_cstr)(void *user, const char *cstr);
  void *user;
}pmath_cstr_writer_info_t;

/**\brief A \a write function for pmath_write() that converts to utf8.
   
   pmath_write() writes output as utf16/ucs2. This function can be used to 
   convert to utf8 on the fly. The \a user parameter to pmath_write must point
   to a \ref pmath_cstr_writer_info_t.
   
   Here is an example on how to use it:
   \code
void my_utf8_output(FILE *f, const char *str){
  fprintf(f, "%s", str);
}

...

pmath_cstr_writer_info_t info;
info.write_cstr = (void(*)(void*,const char*))my_utf8_output;
info.user = stdout; // will be first argument of my_utf8_output

pmath_print(some_object, some_options, pmath_utf8_writer, &info);
   \endcode
 */
PMATH_API 
void pmath_utf8_writer(void *user, const uint16_t *data, int len);

/**\brief A \a write function for pmath_write() that converts to the current 
          console encosing.
   
   This callback function is used like pmath_utf8_writer().
 */
PMATH_API 
void pmath_native_writer(void *user, const uint16_t *data, int len);

/**\brief Short form to convert a C String to a pMath String.
   \memberof pmath_string_t
   \param cstr A C String (zero-terminated char buffer).
   \return A pMath String representing the Latin-1 C string \a cstr.
   
   This is a wrapper macro around pmath_string_insert_latin1().
 */
#define PMATH_C_STRING(cstr) pmath_string_insert_latin1(PMATH_NULL, 0, (cstr), -1)

/**\brief Insert a byte string into a pMath string using a translation array.
   \memberof pmath_string_t
   \param str A pMath String or PMATH_NULL. It will be freed.
   \param inspos The position, at which \c ins should be inserted in \c str.
   \param ins A byte string.
   \param inslen The length of \c ins or -1 if it is zero-terminated.
   \param cp An array of 256 uint16_t values that are used to convert bytes to
          UCS-2 characters.
   \return PMATH_NULL on failure or a pMath String. You must destroy it.

   If \c str is PMATH_NULL, it is assumed to be the empty string.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_string_insert_codepage(
  pmath_string_t  str,
  int             inspos,
  const char     *ins,
  int             inslen,
  const uint16_t *cp);

/**\brief Insert a UCS-2 buffer into a pMath String.
   \memberof pmath_string_t
   \param str A pMath String or PMATH_NULL. It will be freed.
   \param inspos The position, at which \c ins should be inserted in \c str.
   \param ins A uint16_t string. This is \em not a wchar_t string.
   \param inslen The length of \c ins or -1 if it is zero-terminated.
   \return PMATH_NULL on failure or a pMath String. You must destroy it.

   If \c str is PMATH_NULL, it is assumed to be the empty string.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_string_insert_ucs2(
  pmath_string_t  str,
  int             inspos,
  const uint16_t *ins,
  int             inslen);

/**\brief Insert one pMath String into another pMath String.
   \memberof pmath_string_t
   \param str A pMath String or PMATH_NULL. It will be freed.
   \param inspos The position, at which \c ins should be inserted in \c str.
   \param ins A pMath String or PMATH_NULL. It will be freed.
   \return PMATH_NULL on failure or a pMath String. You must destroy it.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_string_insert(
  pmath_string_t str,
  int            inspos,
  pmath_string_t ins);

/**\brief Concatenate two pMath Strings.
   \memberof pmath_string_t
   \param prefix A pMath String. It will be freed.
   \param postfix A pMath String. It will be freed.
   \return PMATH_NULL on failure or a pMath String that consists of \c prefix followed
           by \c postfix. You must destroy it.

   If one of the two strings is PMATH_NULL, the other string will be returned.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_string_concat(
  pmath_string_t prefix,
  pmath_string_t postfix);

/**\brief Extract a substring of a pMath String.
   \memberof pmath_string_t
   \param string A pMath String. It will be freed.
   \param start the substring's start index.
   \param length the substring's length or -1 for the whole substring beginng
          at start.
   \return PMATH_NULL on failure or a pMath String.

   If start or start+length are out of bounds, thy will be truncated, the the
   resulting string's length is not neccesaryly \c length.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_string_part(
  pmath_string_t string,
  int start,
  int length);

/**\brief Get a string's buffer for reading.
   \memberof pmath_string_t
   \param string A pointer to a string.
   \return A pointer to the string's buffer. This buffer is guaranteed to be
           pmath_string_length(str) * sizeof(uint16_t) bytes long.

   Do not forget that pMath strings are not zero-terminated.
 */
 
PMATH_API 
PMATH_ATTRIBUTE_PURE
const uint16_t *pmath_string_buffer(pmath_string_t *string);

/**\brief Get a string's length.
   \memberof pmath_string_t
   \param string A string. It remains valid after the function call, so you have
          to destroy it manually.
   \return The length (in uint16_t characters) of the string. It is never
           negative.
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE
int pmath_string_length(pmath_string_t string);

/**\brief Compare a pMath string with a C string.
   \memberof pmath_string_t
   \param string A string. It wont be freed.
   \param latin1 A C string (zero terminated). 
   \return Whether the two string are equals.
   
   This function is a short form for 
   \code
tmp = PMATH_C_STRING(latin1);
result = pmath_equals(string, tmp);
pmath_unref(tmp);
   \endcode
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_string_equals_latin1(
  pmath_string_t  string, 
  const char     *latin1);

/** @} */

#endif /* __PMATH_CORE__STRING_H__ */
