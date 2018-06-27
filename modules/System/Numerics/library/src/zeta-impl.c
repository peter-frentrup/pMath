#include "stdafx.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyZeta;

#define SIMPLIFY_SYMBOL  pmath_System_Private_AutoSimplifyZeta

PMATH_PRIVATE pmath_t eval_System_Zeta(pmath_expr_t expr) {
#  define ACB_FUNCTION acb_zeta
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}
