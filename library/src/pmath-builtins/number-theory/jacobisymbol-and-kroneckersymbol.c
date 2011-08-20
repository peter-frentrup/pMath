#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


pmath_t builtin_jacobisymbol_and_kroneckersymbol(pmath_expr_t expr) {
  /** The Kronecker symbol is a generalization of Jacobi symbol.
   */
  pmath_mpint_t a, b;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  a = pmath_expr_get_item(expr, 1);
  b = pmath_expr_get_item(expr, 2);
  
  if(pmath_is_int32(a))
    a = _pmath_create_mp_int(PMATH_AS_INT32(a));
    
  if(pmath_is_int32(b))
    b = _pmath_create_mp_int(PMATH_AS_INT32(b));
    
  if(!pmath_is_mpint(a) || !pmath_is_mpint(b)) {
    pmath_unref(a);
    pmath_unref(b);
    return expr;
  }
  
  pmath_unref(expr);
  expr = PMATH_FROM_INT32(mpz_kronecker(PMATH_AS_MPZ(a), PMATH_AS_MPZ(b)));
  
  pmath_unref(a);
  pmath_unref(b);
  return expr;
}
