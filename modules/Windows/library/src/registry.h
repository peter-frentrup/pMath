#ifndef __PMATH_WINDOWS__REGISTRY_H__
#define __PMATH_WINDOWS__REGISTRY_H__

#include <pmath.h>
#include <windows.h>

/** \brief Extract the base and subkey path from a full registry path
    \param out_base Receives the base key upon return. It must not be closed.
    \param fullname The full key path.
    \return The subkey path on success or PMATH_NULL on error.
 */
extern pmath_string_t registry_split_subkey(HKEY *out_base, pmath_string_t fullname);


#endif // __PMATH_WINDOWS__REGISTRY_H__
