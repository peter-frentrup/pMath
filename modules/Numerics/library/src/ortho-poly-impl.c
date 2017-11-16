#include "stdafx.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyOrthoPoly;

#define SIMPLIFY_SYMBOL  pmath_System_Private_AutoSimplifyOrthoPoly

PMATH_PRIVATE pmath_t eval_System_ChebyshevT(pmath_expr_t expr) {
#  define ACB_FUNCTION   acb_hypgeom_chebyshev_t
#    include "acb-impl-twoarg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ChebyshevU(pmath_expr_t expr) {
#  define ACB_FUNCTION   acb_hypgeom_chebyshev_u
#    include "acb-impl-twoarg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_HermiteH(pmath_expr_t expr) {
#  define ACB_FUNCTION   acb_hypgeom_hermite_h
#    include "acb-impl-twoarg.inc"
#  undef ACB_FUNCTION
}
