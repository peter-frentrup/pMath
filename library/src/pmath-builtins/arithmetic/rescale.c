#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>


PMATH_PRIVATE pmath_t builtin_rescale(pmath_expr_t expr) {
  /* Rescale(x, min..max, xmin..xmax)
     Rescale(x, min..max)             == Rescale(x, min..max, 0..1)
     Rescale(list)                    == Map(list, Rescale(#, Min(list)..Max(list))&)
   */
  pmath_t x, min, max, xmin, xmax;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 1, 3);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(exprlen == 1) {
    if(!pmath_is_expr_of(x, PMATH_SYMBOL_LIST)) {
      pmath_message(PMATH_NULL, "nargs", 0);
      pmath_unref(x);
      return expr;
    }
    
    min = pmath_evaluate(FUNC(pmath_ref(PMATH_SYMBOL_MIN), pmath_ref(x)));
    max = pmath_evaluate(FUNC(pmath_ref(PMATH_SYMBOL_MAX), pmath_ref(x)));
    xmin = PMATH_FROM_INT32(0);
    xmax = PMATH_FROM_INT32(1);
  }
  else if(exprlen >= 2) {
    pmath_t range = pmath_expr_get_item(expr, 2);
    
    if(!pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 2)) {
      pmath_unref(x);
      pmath_message(PMATH_NULL, "rtwo", 2, range, PMATH_FROM_INT32(2));
      return expr;
    }
    
    min = pmath_expr_get_item(range, 1);
    max = pmath_expr_get_item(range, 2);
    pmath_unref(range);
    
    if(exprlen == 3) {
      pmath_t range = pmath_expr_get_item(expr, 3);
      
      if(!pmath_is_expr_of_len(range, PMATH_SYMBOL_RANGE, 2)) {
        pmath_unref(x);
        pmath_message(PMATH_NULL, "rtwo", 2, range, PMATH_FROM_INT32(3));
        return expr;
      }
      
      xmin = pmath_expr_get_item(range, 1);
      xmax = pmath_expr_get_item(range, 2);
    }
    else {
      xmin = PMATH_FROM_INT32(0);
      xmax = PMATH_FROM_INT32(1);
    }
  }
  
  pmath_unref(expr);
  {
#define AA     pmath_ref(min)
#define BB     pmath_ref(max)
#define CC     pmath_ref(xmin)
#define DD     pmath_ref(xmax)
#define BB_AA  pmath_ref(diff)
  
    pmath_t diff = pmath_evaluate(MINUS(BB, AA));
    
    expr = PLUS(DIV(TIMES(MINUS(DD, CC), x), BB_AA), DIV(PLUS(TIMES(CC, BB), TIMES3(INT(-1), AA, DD)), BB_AA));
    
    pmath_unref(min);
    pmath_unref(max);
    pmath_unref(xmin);
    pmath_unref(xmax);
    pmath_unref(diff);
  }
  
  return expr;
}
