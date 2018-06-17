#include "stdafx.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyTrigIntegral;

#define SIMPLIFY_SYMBOL  pmath_System_Private_AutoSimplifyTrigIntegral

PMATH_PRIVATE pmath_t eval_System_CoshIntegral(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_chi
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_CosIntegral(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_ci
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_ExpIntegralEi(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_ei
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_LogIntegral(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_chi
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_SinhIntegral(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_shi
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_SinIntegral(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_hypgeom_si
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}
