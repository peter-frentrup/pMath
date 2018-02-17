#include <pmath-core/numbers.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>


PMATH_PRIVATE pmath_t builtin_switch(pmath_expr_t expr) {
  pmath_t value;
  size_t i;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1) {
    pmath_message_argxxx(exprlen, 1, SIZE_MAX);
    return expr;
  }
  
  if((exprlen & 1) == 0) {
    pmath_message(PMATH_NULL, "argct", 1, pmath_integer_new_uiptr(exprlen));
    return expr;
  }
  
  value = pmath_expr_get_item(expr, 1);
  for(i = 2; i < exprlen; i += 2) {
    pmath_t pattern = pmath_expr_get_item(expr, i);
    pattern = pmath_evaluate(pattern);
    if(!_pmath_pattern_validate(pattern)) {
      pmath_unref(value);
      pmath_unref(pattern);
      return expr;
    }
    if(_pmath_pattern_match(value, pattern, NULL)) {
      pmath_unref(value);
      value = pmath_expr_get_item(expr, i + 1);
      pmath_unref(expr);
      return value;
    }
  }
  
  pmath_unref(value);
  return expr;
}
