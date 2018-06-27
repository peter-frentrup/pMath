#include "stdafx.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyZeta;

#define SIMPLIFY_SYMBOL  pmath_System_Private_AutoSimplifyZeta

PMATH_PRIVATE pmath_t eval_System_Zeta(pmath_expr_t expr) {
  if(pmath_expr_length(expr) == 1) {
#    define ACB_FUNCTION acb_zeta
#      include "acb-impl-onearg.inc"
#    undef ACB_FUNCTION
  }
  else if(pmath_expr_length(expr) == 2) {
#    define ACB_FUNCTION  acb_hurwitz_zeta
#      include "acb-impl-twoarg.inc"
#    undef ACB_FUNCTION
  }
  else {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
}
