#include "stdafx.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifySurd;

#define SIMPLIFY_SYMBOL  pmath_System_Private_AutoSimplifySurd

static void arb_surd_pos(arb_t res, const arb_t nonegx, const arb_t posintn, slong prec) {
  if(arf_cmp_ui(arb_midref(posintn), WORD_MAX) <= 0) {
    ulong smalln = (ulong)arf_get_si(arb_midref(posintn), ARF_RND_NEAR);
    arb_root_ui(res, nonegx, smalln, prec);
    return;
  }
  else { // large n: use exp(log(x)/n)
    arb_log(res, nonegx, prec + 4);
    arb_div(res, res, posintn, prec + 4);
    arb_exp(res, res, prec);
  }
}

static void arb_surd(arb_t res, const arb_t x, const arb_t n, slong prec) {
  if(!arb_is_int(n) || arb_is_zero(n)) {
    arb_indeterminate(res);
    return;
  }
  
  if(arb_is_negative(n)) {
    arb_t absn;
    arb_init(absn);
    arb_abs(absn, n);
    arb_surd(res, x, absn, prec + 4);
    arb_inv(res, res, prec);
    arb_clear(absn);
    return;
  }
  
  if(arb_is_nonnegative(x)) {
    arb_surd_pos(res, x, n, prec);
    return;
  }
  
  if(arb_is_int_2exp_si(n, 1)) { // even 
    // message: Surd::noneg
    arb_indeterminate(res);
    return;
  }
  else {
    arb_t absx;
    arb_init(absx);
    arb_abs(absx, x);
    arb_surd_pos(res, absx, n, prec);
    
    if(arb_is_nonnegative(x)) {
      arb_neg(res, res);
    }
    else { // x contains negative and positive numbers
      int midsign = arf_sgn(arb_midref(x));
      if(midsign == 0) {
        arb_t neg_res;
        arb_init(neg_res);
        
        arb_neg(neg_res, res);
        arb_union(res, res, neg_res, prec);
        
        arb_clear(neg_res);
      }
      else if(midsign > 0) { // |upper bound| > |lower bound|
        arb_t abs_neg;
        arb_t surd_abs_neg;
        arb_init(abs_neg);
        arb_init(surd_abs_neg);
        
        arb_neg(abs_neg, x);
        arb_intersection(abs_neg, abs_neg, absx, prec);
        arb_surd_pos(surd_abs_neg, abs_neg, n, prec);
        
        arb_neg(surd_abs_neg, surd_abs_neg);
        
        arb_union(res, res, surd_abs_neg, prec);
        
        arb_clear(surd_abs_neg);
        arb_clear(abs_neg);
      }
      else { // |upper bound| < |lower bound|
        arb_t pos;
        arb_t surd_pos;
        arb_init(pos);
        arb_init(surd_pos);
        
        arb_intersection(pos, x, absx, prec);
        arb_surd_pos(surd_pos, pos, n, prec);
        
        arb_neg(res, res);
        
        arb_union(res, res, surd_pos, prec);
        
        arb_clear(surd_pos);
        arb_clear(pos);
      }
    }
    
    arb_clear(absx);
  }
}

static void acb_surd(acb_t res, const acb_t x, const acb_t n, slong prec) {
  if(!arb_is_zero(acb_imagref(x)) || !arb_is_zero(acb_imagref(n))) {
    acb_indeterminate(res);
    return;
  }
  arb_zero(acb_imagref(res));
  arb_surd(acb_realref(res), acb_realref(x), acb_realref(n), prec);
}

static void acb_cbrt(acb_t res, const acb_t x, slong prec) {
  arb_t three;
  if(!arb_is_zero(acb_imagref(x))) {
    acb_indeterminate(res);
    return;
  }
  arb_init(three);
  arb_set_ui(three, 3);
  
  arb_zero(acb_imagref(res));
  arb_surd(acb_realref(res), acb_realref(x), three, prec);
  
  arb_clear(three);
}

PMATH_PRIVATE pmath_t eval_System_CubeRoot(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_cbrt
#    include "acb-impl-onearg.inc"
#  undef ACB_FUNCTION
}

PMATH_PRIVATE pmath_t eval_System_Surd(pmath_expr_t expr) {
#  define ACB_FUNCTION  acb_surd
#    include "acb-impl-twoarg.inc"
#  undef ACB_FUNCTION
}
