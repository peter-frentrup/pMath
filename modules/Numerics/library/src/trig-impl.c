#include "stdafx.h"
#include "util.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyTrig;

static void acb_csc(acb_t r, const acb_t z, slong prec) {
  acb_sin(r, z, prec);
  acb_inv(r, r, prec);
}

static void acb_sec(acb_t r, const acb_t z, slong prec) {
  acb_cos(r, z, prec);
  acb_inv(r, r, prec);
}

PMATH_PRIVATE pmath_t eval_System_ArcTan(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_atan
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Cos(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cos
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Cot(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cot
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Csc(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_csc
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sec(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sec
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sin(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sin
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Tan(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_tan
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}
