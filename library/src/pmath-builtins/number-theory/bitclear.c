#include <pmath-core/expressions.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>


pmath_t builtin_bitclear(pmath_expr_t expr) {
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
    pmath_unref(expr);
    
    if(pmath_refcount(num) != 1) {
      pmath_mpint_t tmp = num;
      num = _pmath_create_mp_int(0);
      
      if(pmath_is_null(num)) {
        pmath_unref(tmp);
        return PMATH_NULL;
      }
      
      mpz_set(PMATH_AS_MPZ(num), PMATH_AS_MPZ(tmp));
      pmath_unref(tmp);
    }
    
    mpz_clrbit(PMATH_AS_MPZ(num), (unsigned)PMATH_AS_INT32(bit));
    return _pmath_mp_int_normalize(num);
  }
  
  pmath_unref(num);
  return expr;
}
