#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_divideby_or_timesby(pmath_expr_t expr){
/* DivideBy(x, y)     =  x/= y
  TimesByBy(x, y)     =  x*= y
   
   The operations are not atomic! If you want atomic inc/dec, use Synchronize()
   anywhere the symbol is accessed. 
   E.g. Synchronize(x, x/= y)
 */
  size_t exprlen = pmath_expr_length(expr);
  pmath_t rhs, head, lhs, lhs_eval;
  
  if(exprlen != 2){
    pmath_message_argxxx(exprlen, 2, 2);
    return expr;
  }

  lhs = pmath_expr_get_item(expr, 1);
  lhs_eval = pmath_evaluate(pmath_ref(lhs));
  if(pmath_equals(lhs, lhs_eval)){
    pmath_message(PMATH_NULL, "rval", 1, lhs);
    pmath_unref(lhs_eval);
    return expr;
  }
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);

  rhs = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  if(pmath_same(head, PMATH_SYMBOL_DIVIDEBY)){
    rhs = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_POWER), 2,
      rhs,
      PMATH_FROM_INT32(-1));
  }
  
  expr = pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_ASSIGN), 2,
    lhs,
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_TIMES), 2,
      lhs_eval,
      rhs));
  
  return expr;
}
