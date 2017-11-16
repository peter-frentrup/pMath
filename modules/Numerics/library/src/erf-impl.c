#include "stdafx.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyErf;

#define SIMPLIFY_SYMBOL  pmath_System_Private_AutoSimplifyErf

PMATH_PRIVATE pmath_t eval_System_Erf(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_erf
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Erfc(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_erfc
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Erfi(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_erfi
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}
