#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


pmath_t builtin_bitget(pmath_expr_t expr) {
  pmath_mpint_t   num;
  pmath_integer_t bit;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  num = pmath_expr_get_item(expr, 1);
  bit = pmath_expr_get_item(expr, 2);
  
  if(!pmath_is_int32(bit) || PMATH_AS_INT32(bit) < 0) {
    pmath_unref(num);
    pmath_unref(bit);
    return expr;
  }
  
  if(pmath_is_int32(num))
    num = _pmath_create_mp_int(PMATH_AS_INT32(num));
    
  if(pmath_is_mpint(num)) {
    int b = mpz_tstbit(PMATH_AS_MPZ(num), (unsigned)PMATH_AS_INT32(bit));
    
    pmath_unref(expr);
    pmath_unref(num);
    
    return PMATH_FROM_INT32(b);
  }
  
  pmath_unref(num);
  return expr;
}
