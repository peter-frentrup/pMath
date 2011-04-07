#include <pmath-core/numbers-private.h>

#include <pmath-builtins/all-symbols-private.h>


pmath_t builtin_extendedgcd(pmath_expr_t expr){
  size_t exprlen = pmath_expr_length(expr);
  size_t i, j;
  pmath_expr_t factors;
  pmath_mpint_t result;
  
  if(exprlen == 0){
    pmath_unref(expr);
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_LIST), 2,
      PMATH_FROM_INT32(0),
      pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), 0));
  }
  
  factors = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), exprlen);
  result = pmath_expr_get_item(expr, 1);
  if(pmath_is_int32(result))
    result = _pmath_create_mp_int(PMATH_AS_INT32(result));
  
  if(!pmath_is_mpint(result)){
    pmath_unref(result);
    pmath_unref(factors);
    return expr;
  }
  
  factors = pmath_expr_set_item(factors, 1, PMATH_FROM_INT32(1));
  if(exprlen == 1){
    int sgn = mpz_sgn(PMATH_AS_MPZ(result));
    
    pmath_unref(expr);
    if(sgn < 0){
      factors = pmath_expr_set_item(factors, 1, PMATH_FROM_INT32(-1));
      result = pmath_number_neg(_pmath_mp_int_normalize(result));
    }
    else if(sgn == 0){
      factors = pmath_expr_set_item(factors, 1, PMATH_FROM_INT32(0));
    }
    
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_LIST), 2,
      _pmath_mp_int_normalize(result),
      factors);
  }
  
  for(i = 2;i <= exprlen;++i){
    pmath_mpint_t item = pmath_expr_get_item(expr, i);
    pmath_mpint_t g, s, t;
    
    if(pmath_is_int32(item))
      item = _pmath_create_mp_int(PMATH_AS_INT32(item));
    
    if(!pmath_is_mpint(result)){
      pmath_unref(item);
      pmath_unref(result);
      pmath_unref(factors);
      return expr;
    }
    
    g = _pmath_create_mp_int(0);
    s = _pmath_create_mp_int(0);
    t = _pmath_create_mp_int(0);
    if(pmath_is_null(g) || pmath_is_null(s) || pmath_is_null(t)){
      pmath_unref(g);
      pmath_unref(s);
      pmath_unref(t);
      pmath_unref(item);
      pmath_unref(result);
      pmath_unref(factors);
      return expr;
    }
    
    mpz_gcdext(
      PMATH_AS_MPZ(g),
      PMATH_AS_MPZ(s),
      PMATH_AS_MPZ(t),
      PMATH_AS_MPZ(result),
      PMATH_AS_MPZ(item));
    
    pmath_unref(item);
    pmath_unref(result);
    result = g;
    
    s = _pmath_mp_int_normalize(s);
    factors = pmath_expr_set_item(factors, i, _pmath_mp_int_normalize(t));
    for(j = 1;j < i;++j){
      item = pmath_expr_extract_item(factors, j);
      item = _mul_ii(item, pmath_ref(s));
      if(pmath_is_null(item)){
        pmath_unref(s);
        pmath_unref(result);
        pmath_unref(factors);
        return expr;
      }
      factors = pmath_expr_set_item(factors, j, item);
    }
    
    pmath_unref(s);
  }
  
  pmath_unref(expr);
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_LIST), 2,
    _pmath_mp_int_normalize(result),
    factors);
}
