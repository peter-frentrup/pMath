#include <pmath-core/expressions.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

/** \brief Replace small floating point numbers by integer 0.
    \param obj       An expression. It will be freed.
    \param tolerance Maximum absulute value of floating point numbers in \a obj that should be replaced by integer 0.
    \return A new expression with the replacements done.
 */
static pmath_t chop_arb_tolerance(pmath_t obj, const arb_t tolerance) {
  if(pmath_is_double(obj)) {
    if(arf_cmpabs_d(arb_midref(tolerance), PMATH_AS_DOUBLE(obj)) >= 0)
      return PMATH_FROM_INT32(0);
    return obj;
  }
  
  if(pmath_is_mpfloat(obj)) {
    if(arb_le(PMATH_AS_ARB(obj), tolerance)) {
      pmath_unref(obj);
      return PMATH_FROM_INT32(0);
    }
    return obj;
  }
  
  if(pmath_is_expr(obj)) {
    size_t i;
    size_t len = pmath_expr_length(obj);
    
    for(i = 0; i <= len; ++i) {
      pmath_t item = pmath_expr_extract_item(obj, i);
      item = chop_arb_tolerance(item, tolerance);
      obj = pmath_expr_set_item(obj, i, item);
    }
  }
  
  return obj;
}

static pmath_t chop(
  pmath_t obj,               // will be freed
  pmath_number_t tolerance   // wont be freed
) {
  if(pmath_is_mpfloat(tolerance)) 
    return chop_arb_tolerance(obj, PMATH_AS_ARB(tolerance));
  
  if(pmath_is_number(tolerance)) {
    arb_t tol;
    arb_init(tol);
    _pmath_number_get_arb(tol, tolerance, DBL_MANT_DIG);
    obj = chop_arb_tolerance(obj, tol);
    arb_clear(tol);
    return obj;
  }
  
  return obj;
}

PMATH_PRIVATE
pmath_t builtin_chop(pmath_expr_t expr) {
  pmath_t obj;
  pmath_number_t tolerance;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  if(exprlen == 2) {
    tolerance = pmath_expr_get_item(expr, 2);
    
    if(!pmath_is_number(tolerance) || pmath_number_sign(tolerance) < 0) {
      pmath_unref(tolerance);
      
      pmath_message(
        PMATH_NULL, "numn", 2,
        PMATH_FROM_INT32(2),
        pmath_ref(expr));
        
      return expr;
    }
  }
  else
    tolerance = PMATH_FROM_DOUBLE(1e-10);
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  obj = chop(obj, tolerance);
  
  pmath_unref(tolerance);
  return obj;
}
