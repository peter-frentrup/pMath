#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_Assign;
extern pmath_symbol_t pmath_System_Decrement;
extern pmath_symbol_t pmath_System_Increment;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_PostDecrement;
extern pmath_symbol_t pmath_System_Times;

PMATH_PRIVATE pmath_t builtin_dec_or_inc_or_postdec_or_postinc(pmath_expr_t expr) {
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
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  lhs = pmath_expr_get_item(expr, 1);
  lhs_eval = pmath_evaluate(pmath_ref(lhs));
  if(pmath_equals(lhs, lhs_eval)) {
    pmath_message(PMATH_NULL, "rval", 1, lhs);
    pmath_unref(lhs_eval);
    return expr;
  }
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
  if(exprlen == 2) {
    delta = pmath_expr_get_item(expr, 2);
    if( pmath_same(head, pmath_System_Decrement) ||
        pmath_same(head, pmath_System_PostDecrement))
    {
      delta = pmath_expr_new_extended(
                pmath_ref(pmath_System_Times), 2,
                PMATH_FROM_INT32(-1),
                delta);
    }
  }
  else if( pmath_same(head, pmath_System_Decrement) ||
           pmath_same(head, pmath_System_PostDecrement))
  {
    delta = PMATH_FROM_INT32(-1);
  }
  else
    delta = PMATH_FROM_INT32(1);
    
  pmath_unref(expr);
  
  expr = pmath_expr_new_extended(
           pmath_ref(pmath_System_Assign), 2,
           lhs,
           pmath_expr_new_extended(
             pmath_ref(pmath_System_Plus), 2,
             pmath_ref(lhs_eval),
             delta));
             
  if( pmath_same(head, pmath_System_Decrement) ||
      pmath_same(head, pmath_System_Increment))
  {
    pmath_unref(lhs_eval);
    return expr;
  }
  
  pmath_unref(pmath_evaluate(expr));
  return lhs_eval;
}
