#ifndef __MODULE__PMATH_NUMERICS__STDAFX_H__
#define __MODULE__PMATH_NUMERICS__STDAFX_H__


#include <stdint.h>
#include <stdlib.h>

#ifdef _MSC_VER
// Needed by mpir/gmp to use dllimport declarations
#  define MSC_USE_DLL

#  pragma warning(push)
// In mpz_get_ui(): Converting 'mp_limb_t' to 'unsigned long': possible data loss.
#    pragma warning(disable: 4244)
#    include <gmp.h>
#    include <arb.h>
#    include <acb.h>
#  pragma warning(pop)
#else
#  include <gmp.h>
#  include <arb.h>
#  include <acb.h>
#endif // _MSC_VER

#include <pmath.h>

#include <acb_hypgeom.h>


#endif // __MODULE__PMATH_NUMERICS__STDAFX_H__
