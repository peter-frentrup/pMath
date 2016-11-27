#ifndef __PMATH_UTIL__IO_VARINT_PRIVATE_H__
#define __PMATH_UTIL__IO_VARINT_PRIVATE_H__

#include <pmath-core/numbers-private.h>

/**\brief Write an integer in variable width binary format.
   \param file A writable binary file. It wont be freed.
   \param value An integer. It will be freed.
   
   The most significant bit (msb) of every byte indicates whether more bytes follow.
   Value is stored in little endian order in the lower 7 bits per byte.
   Least significant bit of the first byte determines sign. If it is set, the actual value 
   is -1-absValue, otherwise absValue.
 */
PMATH_PRIVATE
void _pmath_serialize_raw_integer(pmath_t file, pmath_integer_t value);

PMATH_PRIVATE
void _pmath_serialize_raw_integer_ui(pmath_t file, uintptr_t value);

PMATH_PRIVATE
void _pmath_serialize_raw_integer_si(pmath_t file, intptr_t value);

PMATH_PRIVATE
pmath_integer_t _pmath_deserialize_raw_integer(pmath_t file);

PMATH_PRIVATE
pmath_bool_t _pmath_deserialize_raw_integer_ui(pmath_t file, uintptr_t *value);

PMATH_PRIVATE
pmath_bool_t _pmath_deserialize_raw_integer_si(pmath_t file, intptr_t *value);

#endif // __PMATH_UTIL__IO_VARINT_PRIVATE_H__
