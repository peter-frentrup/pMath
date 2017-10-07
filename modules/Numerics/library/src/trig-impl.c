#include "stdafx.h"
#include "util.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyTrig;

PMATH_PRIVATE pmath_t eval_System_Cos(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_cos
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
