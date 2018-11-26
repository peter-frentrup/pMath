#include "stdafx.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyArithmeticGeometricMean;

#define SIMPLIFY_SYMBOL  pmath_System_Private_AutoSimplifyArithmeticGeometricMean

static void acb_agm_2(acb_t res, const acb_t a, const acb_t b, slong prec) {
  acb_t ba;
  acb_t agm1_ba;
  
  if(acb_is_one(a)) {
    acb_agm1(res, b, prec);
    return;
  }
  if(acb_is_one(b)) {
    acb_agm1(res, a, prec);
    return;
  }
   
  // agm(a,b) = a agm(1, b/a)
  acb_init(ba);
  acb_init(agm1_ba);
  acb_div(ba, b, a, prec);
  acb_agm1(agm1_ba, ba, prec);
  /* acb_mul() does not allow aliasing output wiht input arguments */
  acb_mul(res, agm1_ba, a, prec); 
  acb_clear(agm1_ba);
  acb_clear(ba);
}

PMATH_PRIVATE pmath_t eval_System_ArithmeticGeometricMean(pmath_expr_t expr) {
#    define ACB_FUNCTION  acb_agm_2
#      include "acb-impl-twoarg.inc"
#    undef ACB_FUNCTION
}
