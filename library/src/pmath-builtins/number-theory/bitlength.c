#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


pmath_t builtin_bitlength(pmath_expr_t expr) {
  pmath_mpint_t item;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  item = pmath_expr_get_item(expr, 1);
  if(pmath_is_int32(item)) {
    if( PMATH_AS_INT32(item) == 0 ||
        PMATH_AS_INT32(item) == -1)
    {
      pmath_unref(expr);
      return PMATH_FROM_INT32(0);
    }
    
    item = _pmath_create_mp_int(PMATH_AS_INT32(item));
  }
  
  if(pmath_is_mpint(item)) {
    size_t size;
    
    if(mpz_sgn(PMATH_AS_MPZ(item)) < 0) {
    
      pmath_mpint_t tmp = item;
      item = _pmath_create_mp_int(0);
      
      if(pmath_is_null(item)) {
        pmath_unref(tmp);
        return expr;
      }
      
      mpz_com(PMATH_AS_MPZ(item), PMATH_AS_MPZ(tmp));
      
      pmath_unref(tmp);
    }
    
    size = mpz_sizeinbase(PMATH_AS_MPZ(item), 2);
    
    pmath_unref(item);
    pmath_unref(expr);
    return PMATH_FROM_INT32((int32_t)size);
  }
  
  pmath_unref(item);
  return expr;
}
