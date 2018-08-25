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

static void acb_hankel_h1(acb_t res, const acb_t nu, const acb_t z, slong prec) {
  acb_t res_y;
  acb_init(res_y);
  acb_hypgeom_bessel_jy(res, res_y, nu, z, prec);
  acb_mul_onei(res_y, res_y);
  acb_add(res, res, res_y, prec);
  acb_clear(res_y);
}

static void acb_hankel_h2(acb_t res, const acb_t nu, const acb_t z, slong prec) {
  acb_t res_y;
  acb_init(res_y);
  acb_hypgeom_bessel_jy(res, res_y, nu, z, prec);
  acb_div_onei(res_y, res_y);
  acb_add(res, res, res_y, prec);
  acb_clear(res_y);
}

PMATH_PRIVATE pmath_t eval_System_HankelH1(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hankel_h1
#    include "acb-impl-twoarg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_HankelH2(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hankel_h2
#    include "acb-impl-twoarg.inc"
#  undef ACB_FUNCTION
}
