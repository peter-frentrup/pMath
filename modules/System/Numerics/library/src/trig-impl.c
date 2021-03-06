#include "stdafx.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyTrig;

#define SIMPLIFY_SYMBOL  pmath_System_Private_AutoSimplifyTrig

PMATH_PRIVATE pmath_t eval_System_ArcCos(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_acos
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcCosh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_acosh
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcSin(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_asin
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcSinh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_asinh
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcTan(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_atan
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ArcTanh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_atanh
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Cos(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cos
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Cosh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cosh
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Cot(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cot
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Coth(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_coth
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Csc(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_csc
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Csch(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_csch
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sec(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sec
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sech(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sech
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sin(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sin
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sinc(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sinc
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Sinh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_sinh
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Tan(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_tan
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Tanh(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_tanh
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}
