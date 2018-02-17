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

static void acb_normalized_fresnel_s(acb_t res, const acb_t z, slong prec) {
  acb_hypgeom_fresnel(res, NULL, z, 1, prec);
}

static void acb_normalized_fresnel_c(acb_t res, const acb_t z, slong prec) {
  acb_hypgeom_fresnel(NULL, res, z, 1, prec);
}

PMATH_PRIVATE pmath_t eval_System_FresnelC(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_normalized_fresnel_c
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_FresnelS(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_normalized_fresnel_s
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}
