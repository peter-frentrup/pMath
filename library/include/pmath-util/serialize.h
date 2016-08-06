#ifndef __PMATH_UTIL__SERIALIZE_H__
#define __PMATH_UTIL__SERIALIZE_H__

#include <pmath-core/objects-inline.h>

/**\brief (De-)Serialization error codes.
 */
typedef enum {
  PMATH_SERIALIZE_OK         = 0, ///< No error occured.
  PMATH_SERIALIZE_NO_MEMORY  = 1,
  PMATH_SERIALIZE_BAD_OBJECT = 2, ///< The object cannot be serialized (e.g. custom objects)
  PMATH_SERIALIZE_EOF        = 3, ///< Unexpected end of file.
  PMATH_SERIALIZE_BAD_BYTE   = 4, ///< Unexpected byte.
  PMATH_SERIALIZE_BAD_REF    = 5, ///< Unknown back reference.
} pmath_serialize_error_t;

/**\brief Write an object to a binary file.
   \param file A \ref file_api "file object". It wont be freed.
   \param object A pMath object. It will be freed.
   \param flags Options bitset. Must be 0.
   \return An error code.
 */
PMATH_API
pmath_serialize_error_t pmath_serialize(
  pmath_t file,
  pmath_t object,
  int     flags);

/**\brief Write an object to a binary file.
   \param file A \ref file_api "file object". It wont be freed.
   \param error Where to put the error code (optional).
   \return The deserialized object.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_deserialize(
  pmath_t                  file,
  pmath_serialize_error_t *error);

#endif /* __PMATH_UTIL__SERIALIZE_H__ */
