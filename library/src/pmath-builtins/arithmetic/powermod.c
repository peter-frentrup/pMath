#include <pmath-core/numbers-private.h>
#include <pmath-core/expressions.h>

#include <pmath-util/messages.h>


static pmath_t powermod(pmath_expr_t expr, pmath_mpint_t base, pmath_mpint_t exponent, pmath_mpint_t mod) {
  pmath_mpint_t tmp = _pmath_create_mp_int(0);
  if(pmath_same(tmp, PMATH_NULL)) {
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_unref(mod);
    return expr;
  }
  
  if(mpz_sgn(PMATH_AS_MPZ(exponent))  < 0) {
    mpz_gcd(PMATH_AS_MPZ(tmp), PMATH_AS_MPZ(base), PMATH_AS_MPZ(mod));
    
    if(mpz_cmp_ui(PMATH_AS_MPZ(tmp), 1) != 0) {
      pmath_message(PMATH_NULL, "ninv", 2, base, mod);
      pmath_unref(exponent);
      return expr;
    }
  }
  
  mpz_powm(PMATH_AS_MPZ(tmp), PMATH_AS_MPZ(base), PMATH_AS_MPZ(exponent), PMATH_AS_MPZ(mod));
  pmath_unref(base);
  pmath_unref(exponent);
  pmath_unref(mod);
  pmath_unref(expr);
  return _pmath_mp_int_normalize(tmp);
}

PMATH_PRIVATE pmath_t builtin_powermod(pmath_expr_t expr) {
/* PowerMod(base, exponent, mod) gives base^exponent modulu mod.
 */
  pmath_mpint_t base, exponent, mod;
  
  if(pmath_expr_length(expr) != 3) {
    pmath_message_argxxx(pmath_expr_length(expr), 3, 3);
    return expr;
  }
  
  mod = pmath_expr_get_item(expr, 3);
  if(pmath_same(mod, PMATH_FROM_INT32(0))) {
    pmath_message(PMATH_NULL, "divz", 2, mod, pmath_ref(expr));
    return expr;
  }
  
  base = pmath_expr_get_item(expr, 1);
  exponent = pmath_expr_get_item(expr, 2);
  
  if(pmath_is_int32(base)) 
    base = _pmath_create_mp_int(PMATH_AS_INT32(base));
  
  if(pmath_is_int32(exponent)) 
    exponent = _pmath_create_mp_int(PMATH_AS_INT32(exponent));
  
  if(pmath_is_int32(mod)) 
    mod = _pmath_create_mp_int(PMATH_AS_INT32(mod));
  
  if(pmath_is_mpint(base) && pmath_is_mpint(exponent) && pmath_is_mpint(mod)) 
    return powermod(expr, base, exponent, mod);
  
  if(pmath_is_numeric(base) && pmath_is_numeric(exponent) && pmath_is_numeric(mod)) {
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_unref(mod);
    pmath_message(PMATH_NULL, "pmod", 1, pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(base);
  pmath_unref(exponent);
  pmath_unref(mod);
  return expr;
}
