#include "stdafx.h"
#include "util.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyTrig;

static void acb_csc(acb_t r, const acb_t z, slong prec) {
  acb_sin(r, z, prec);
  acb_inv(r, r, prec);
}

static void acb_csch(acb_t r, const acb_t z, slong prec) {
  acb_sinh(r, z, prec);
  acb_inv(r, r, prec);
}

static void acb_sec(acb_t r, const acb_t z, slong prec) {
  acb_cos(r, z, prec);
  acb_inv(r, r, prec);
}

static void acb_sech(acb_t r, const acb_t z, slong prec) {
  acb_cosh(r, z, prec);
  acb_inv(r, r, prec);
}

PMATH_PRIVATE pmath_t eval_System_ArcCos(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_acos
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcCosh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_acosh
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcSin(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_asin
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcSinh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_asinh
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcTan(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_atan
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcTanh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_atanh
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Cos(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cos
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Cosh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cosh
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Cot(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cot
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Coth(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_coth
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Csc(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_csc
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Csch(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_csch
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sec(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sec
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sech(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sech
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sin(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sin
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sinh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sinh
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Tan(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_tan
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Tanh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_tanh
#    include "trig-impl.inc"
#  undef ACB_FUNCTION
}
