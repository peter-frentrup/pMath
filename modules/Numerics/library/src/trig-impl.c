#include "stdafx.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyTrig;

#define SIMPLIFY_SYMBOL  pmath_System_Private_AutoSimplifyTrig

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
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcCosh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_acosh
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcSin(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_asin
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcSinh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_asinh
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcTan(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_atan
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcTanh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_atanh
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Cos(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cos
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Cosh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cosh
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Cot(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cot
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Coth(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_coth
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Csc(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_csc
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Csch(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_csch
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sec(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sec
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sech(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sech
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sin(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sin
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sinh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sinh
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Tan(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_tan
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Tanh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_tanh
#    include "simple-acb-impl.inc"
#  undef ACB_FUNCTION
}
