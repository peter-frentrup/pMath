#ifndef PMATH___UTIL__DATA_TYPES__BYTE_ARRAYS_H__INCLUDED
#define PMATH___UTIL__DATA_TYPES__BYTE_ARRAYS_H__INCLUDED

#include <pmath-core/packed-arrays.h>
#include <pmath-core/strings.h>

/**\defgroup byte_arrays Byte Arrays
   \ingroup expressions
   \brief Compact lists of integers between 0 and 255.
  
  A byte array is an expression whose items are single bytes 
  (integers between 0 and 255) with efficient internal representation.
 */
 
/**\class pmath_byte_array_t
   \extends pmath_expr_t
   \brief The Byte Array class.
   
   When seen as a mere expression object, a byte array has the form
   <tt>ByteArray(b1, b2, ..., bN)</tt> but is usually displayed more compactly.
 */
typedef pmath_expr_t pmath_byte_array_t;


/** \brief Test whether an expression is a byte array expression.
    \memberof pmath_byte_array_t
    \param obj An arbitrary pMath object.
    \return TRUE if \a obj is a an expressionn with head <tt>ByteArray</tt> that is moreover 
            represented internally as a \ref pmath_byte_array_t.
 */
PMATH_API pmath_bool_t pmath_is_byte_array(pmath_t obj);


/**\brief Creates a new ByteArray(...) expression.
   \memberof pmath_byte_array_t
   \param blob The backing store of the bytes. It will be freed.
   \param start The zero-based start offset of the bytes.
   \param length The number of bytes.
   \return A new byte array expression or PMATH_NULL on error.
 */
PMATH_API pmath_byte_array_t pmath_byte_array_new(
  pmath_blob_t blob,
  size_t       start,
  size_t       length);


/**\brief Decodes a Base64 string to a ByteArray(...) expression.
   \memberof pmath_byte_array_t
   \param str A string containing valid Base64 characters and possibly whitespace characters. It will be freed.
   \return A new byte array expression or PMATH_NULL on error.
 */
PMATH_API pmath_byte_array_t pmath_byte_array_new_from_base64(pmath_string_t str);

/**\brief Access the internal data of the byte array for reading.
   \memberof pmath_byte_array_t
   \param ba  A byte array. It wont be freed.
   \return A read-only pointer to the internal data.
   \see pmath_blob_read
 */
PMATH_API const uint8_t *pmath_byte_array_read(pmath_byte_array_t ba);

  
#endif // PMATH___UTIL__DATA_TYPES__BYTE_ARRAYS_H__INCLUDED
