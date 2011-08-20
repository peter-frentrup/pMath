#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


pmath_t builtin_bitnot(pmath_expr_t expr) {
  pmath_mpint_t item;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  item = pmath_expr_get_item(expr, 1);
  if(pmath_is_int32(item))
    item = _pmath_create_mp_int(PMATH_AS_INT32(item));
    
  if(pmath_is_mpint(item)) {
    pmath_mpint_t result = _pmath_create_mp_int(0);
    
    if(!pmath_is_null(result)) {
      pmath_unref(expr);
      
      mpz_com(PMATH_AS_MPZ(result), PMATH_AS_MPZ(item));
      
      pmath_unref(item);
      return _pmath_mp_int_normalize(result);
    }
  }
  
  pmath_unref(item);
  return expr;
}
