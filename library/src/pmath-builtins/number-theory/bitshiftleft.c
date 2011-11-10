#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>


pmath_t builtin_bitshiftleft(pmath_expr_t expr) {
  pmath_mpint_t   num;
  pmath_integer_t count;
  
  if(pmath_expr_length(expr) == 1) {
    count = PMATH_FROM_INT32(1);
  }
  else if(pmath_expr_length(expr) == 2) {
    count = pmath_expr_get_item(expr, 2);
  }
  else {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
  
  num = pmath_expr_get_item(expr, 1);
  
  if(!pmath_is_int32(count)) {
    pmath_unref(num);
    pmath_unref(count);
    return expr;
  }
  
  if(pmath_is_int32(num))
    num = _pmath_create_mp_int(PMATH_AS_INT32(num));
    
  if(pmath_is_mpint(num)) {
    pmath_mpint_t result = _pmath_create_mp_int(0);
    
    if(!pmath_is_null(result)) {
      pmath_unref(expr);
      
      if(PMATH_AS_INT32(count) >= 0) {
        mpz_mul_2exp(PMATH_AS_MPZ(result), PMATH_AS_MPZ(num), (unsigned)PMATH_AS_INT32(count));
        
        pmath_unref(num);
        return _pmath_mp_int_normalize(result);
      }
      
      mpz_tdiv_q_2exp(PMATH_AS_MPZ(result), PMATH_AS_MPZ(num), (unsigned)-PMATH_AS_INT32(count));
      
      pmath_unref(num);
      return _pmath_mp_int_normalize(result);
    }
  }
  
  if(pmath_is_number(num)) {
    pmath_unref(expr);
    
    // Quotient(num * 2^count, 1)
    expr = TIMES(num, POW(INT(2), count));
    
    return expr;
  }
  
  pmath_unref(num);
  return expr;
}
