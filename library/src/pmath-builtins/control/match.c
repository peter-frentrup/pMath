#include <pmath-language/patterns-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_True;

PMATH_PRIVATE pmath_t builtin_match(pmath_expr_t expr){
  /* Match(obj, pattern)
   */
  pmath_t obj, pat;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pat = pmath_expr_get_item(expr, 2);
  if(!_pmath_pattern_validate(pat)) {
    pmath_unref(obj);
    pmath_unref(pat);
    return expr;
  }

  pmath_unref(expr);
  
  if(_pmath_pattern_match(obj, pat, NULL)){
    pmath_unref(obj);
    return pmath_ref(pmath_System_True);
  }

  pmath_unref(obj);
  return pmath_ref(pmath_System_False);
}
