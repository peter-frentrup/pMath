#ifndef __PMATH_WINDOWS__ERROR_UTIL_H__
#define __PMATH_WINDOWS__ERROR_UTIL_H__

#include <pmath.h>
#include <windows.h>

/** Convert a pMath string to a BSTR and free the pMath string.
 */
extern BSTR string_to_bstr_free(pmath_string_t str);

/** Convert a BSTR to a pMath string. The BSTR is not freed.
 */
extern pmath_string_t bstr_to_string(BSTR bstr);

/** Test whether a HRESULT indicates success and print a message if not.
 */
extern pmath_bool_t check_succeeded(HRESULT hr);


#endif // __PMATH_WINDOWS__ERROR_UTIL_H__
