#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-builtins/all-symbols-private.h>


pmath_t builtin_bitor(pmath_expr_t expr){
  size_t exprlen = pmath_expr_length(expr);
  size_t i, j;
  
  if(exprlen == 0){
    pmath_unref(expr);
    return PMATH_FROM_INT32(0);
  }
  
  if(exprlen == 1){
    pmath_t a = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return a;
  }
  
  for(i = 1;i <= exprlen;++i){
    pmath_mpint_t a = pmath_expr_get_item(expr, i);
    
    if(pmath_is_int32(a))
      a = _pmath_create_mp_int(PMATH_AS_INT32(a));
    
    if(pmath_is_mpint(a)){
      pmath_bool_t have_symbolic = i > 1;
      pmath_bool_t have_undefined = FALSE;
      
      for(j = i+1;j <= exprlen && mpz_cmp_si(PMATH_AS_MPZ(a), -1) != 0;++j){
        pmath_mpint_t b = pmath_expr_get_item(expr, j);
        
        if(pmath_is_int32(b))
          b = _pmath_create_mp_int(PMATH_AS_INT32(b));
        
        if(pmath_is_mpint(a)){
          pmath_mpint_t c = _pmath_create_mp_int(0);
          have_undefined = TRUE;
          
          if(pmath_is_null(c)){
            pmath_unref(a);
            pmath_unref(b);
            return expr;
          }
          
          mpz_ior(
            PMATH_AS_MPZ(c),
            PMATH_AS_MPZ(a),
            PMATH_AS_MPZ(b));
          
          pmath_unref(a);
          a = c;
          expr = pmath_expr_set_item(expr, j, PMATH_UNDEFINED);
        }
        else
          have_symbolic = TRUE;
        
        pmath_unref(b);
      }
      
      if(mpz_cmp_si(PMATH_AS_MPZ(a), -1) == 0){
        pmath_unref(a);
        pmath_unref(expr);
        return PMATH_FROM_INT32(-1);
      }
      
      if(mpz_sgn(PMATH_AS_MPZ(a)) == 0){
        pmath_unref(a);
        
        if(have_symbolic){
          expr = pmath_expr_set_item(expr, i, PMATH_UNDEFINED);
          return _pmath_expr_shrink_associative(expr, PMATH_UNDEFINED); 
        }
        
        pmath_unref(expr);
        return PMATH_FROM_INT32(0);
      }
      
      if(!have_undefined){
        pmath_unref(a);
        return expr;
      }
      
      a = _pmath_mp_int_normalize(a);
      if(have_symbolic){
        expr = pmath_expr_set_item(expr, i, a);
        return _pmath_expr_shrink_associative(expr, PMATH_UNDEFINED); 
      }
      
      pmath_unref(expr);
      return a;
    }
    
    pmath_unref(a);
  }
  
  return expr;
}
