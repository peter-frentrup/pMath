#include "stdafx.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyBessel;

#define SIMPLIFY_SYMBOL  pmath_System_Private_AutoSimplifyBessel

PMATH_PRIVATE pmath_t eval_System_BesselI(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_bessel_i
#    include "acb-impl-twoarg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_BesselJ(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_bessel_j
#    include "acb-impl-twoarg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_BesselK(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_bessel_k
#    include "acb-impl-twoarg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_BesselY(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_bessel_y
#    include "acb-impl-twoarg.inc"
#  undef ACB_FUNCTION
}
