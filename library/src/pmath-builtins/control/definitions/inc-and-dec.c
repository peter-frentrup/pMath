#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_dec_or_inc_or_postdec_or_postinc(pmath_expr_t expr){
/* PostIncrement(x)   =  x++
   PostIncrement(x, y)
   Increment(x)       =  ++x
   Increment(x,y)     =  x+= y

   PostDecrement(x)   =  x--
   PostDecrement(x, y)
   Decrement(x)       =  --x
   Decrement(x,y)     =  x-= y

   The operations are not atomic! If you want atomic inc/dec, use Synchronize(). 
   anywhere the symbol is accessed. 
   E.g. Synchronize(x, x+= y)
 */
  size_t exprlen = pmath_expr_length(expr);
  pmath_t delta, head, lhs, lhs_eval;
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }

  lhs = pmath_expr_get_item(expr, 1);
  lhs_eval = pmath_evaluate(pmath_ref(lhs));
  if(pmath_equals(lhs, lhs_eval)){
    pmath_message(NULL, "rval", 1, lhs);
    pmath_unref(lhs_eval);
    return expr;
  }
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);

  if(exprlen == 2){
    delta = pmath_expr_get_item(expr, 2);
    if(head == PMATH_SYMBOL_DECREMENT
    || head == PMATH_SYMBOL_POSTDECREMENT)
      delta = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TIMES), 2,
        pmath_integer_new_si(-1),
        delta);
  }
  else if(head == PMATH_SYMBOL_DECREMENT
  ||      head == PMATH_SYMBOL_POSTDECREMENT)
    delta = pmath_integer_new_si(-1);
  else
    delta = pmath_integer_new_si(1);
  pmath_unref(expr);

  expr = pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_ASSIGN), 2,
    lhs,
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_PLUS), 2,
      pmath_ref(lhs_eval),
      delta));

  if(head == PMATH_SYMBOL_DECREMENT
  || head == PMATH_SYMBOL_INCREMENT){
    pmath_unref(lhs_eval);
    return expr;
  }

  pmath_unref(pmath_evaluate(expr));
  return lhs_eval;
}
