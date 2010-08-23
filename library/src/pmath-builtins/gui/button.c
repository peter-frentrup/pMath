#include <pmath-util/evaluation.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

PMATH_PRIVATE pmath_t builtin_button(pmath_expr_t expr){
  size_t i;
  
  if(pmath_expr_length(expr) < 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  for(i = pmath_expr_length(expr);i > 2;--i){
    expr = pmath_expr_set_item(
      expr, i,
      pmath_evaluate(
        pmath_expr_get_item(expr, i)));
  }
  
  pmath_unref(pmath_options_extract(expr, 2));
  
  return expr;
}
