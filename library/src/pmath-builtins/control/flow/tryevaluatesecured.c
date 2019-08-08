#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>
#include <pmath-util/security.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_internal_tryevaluatesecured(pmath_expr_t expr) {
  /* Internal`TryEvaluateSecured(expr, level)
   */
  pmath_t obj;
  pmath_security_level_t level;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  obj = pmath_evaluate(pmath_expr_get_item(expr, 2));
  if(!_pmath_security_level_from_expr(&level, obj)) {
    pmath_message(PMATH_NULL, "seclvl", 2, obj);
    return expr;
  }
  pmath_unref(obj);
  
  obj = pmath_expr_extract_item(expr, 1);
  pmath_unref(expr);
  
  obj = pmath_evaluate_secured(obj, level);
  
  obj = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_HOLDCOMPLETE), 1, obj);
  return obj;
}
