#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>


extern pmath_symbol_t pmath_System_IntegerPart;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_Times;

PMATH_PRIVATE pmath_t builtin_fractionalpart(pmath_expr_t expr) {
  pmath_t x, ix;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  
  ix = FUNC(pmath_ref(pmath_System_IntegerPart), pmath_ref(x));
  ix = pmath_evaluate(ix);
  if(!pmath_is_expr_of_len(ix, pmath_System_IntegerPart, 1)){
    pmath_unref(expr);
    return MINUS(x, ix);
  }
  
  pmath_unref(x);
  pmath_unref(ix);
  return expr;
}
