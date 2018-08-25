#ifndef __PMATH_BUILTINS__IO_PRIVATE_H__
#define __PMATH_BUILTINS__IO_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

#include <pmath-core/expressions.h>
#include <pmath-core/strings.h>

/* This header exports all definitions of the sources in
   src/pmath-builtins/io/
 */

// 0 = invalid, -1 = LE, 1 = BE
PMATH_PRIVATE int _pmath_get_byte_ordering(pmath_t head, pmath_expr_t options);

PMATH_PRIVATE pmath_bool_t _pmath_file_check(pmath_t file, int properties);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t _pmath_get_directory(void);


PMATH_PRIVATE void _init_pmath_native_encoding(void); // called from _pmath_strings_init()

extern PMATH_PRIVATE const char *_pmath_native_encoding;
extern PMATH_PRIVATE pmath_bool_t _pmath_native_encoding_is_utf8;

#endif /* __PMATH_BUILTINS__IO_PRIVATE_H__ */
