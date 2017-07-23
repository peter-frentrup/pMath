#ifndef __PMATH_WINDOWS__ERROR_UTIL_H__
#define __PMATH_WINDOWS__ERROR_UTIL_H__

#include <pmath.h>
#include <objbase.h>

/** Convert a pMath string to a BSTR and free the pMath string.
 */
extern BSTR string_to_bstr_free(pmath_string_t str);

/** Convert a BSTR to a pMath string. The BSTR is not freed.
 */
extern pmath_string_t bstr_to_string(BSTR bstr);


/** Convert a pMath string to a GUID. The string will not be freed.
 */
extern pmath_bool_t string_to_guid(pmath_string_t string, GUID *guid);

/** Convert a GUID to a pMath string.
 */
extern pmath_string_t guid_to_string(const GUID *guid);

/** Test whether a HRESULT indicates success and print a message if not.
 */
extern pmath_bool_t check_succeeded(HRESULT hr);

/** Test whether a win32 error code indicates success and print a message if not.
 */
extern pmath_bool_t check_succeeded_win32(DWORD error_code);


#endif // __PMATH_WINDOWS__ERROR_UTIL_H__
