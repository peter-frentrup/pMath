#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/flow-private.h>


extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_True;

PMATH_PRIVATE pmath_t builtin_while(pmath_expr_t expr) {
  /* While(cond, block)
   */
  pmath_thread_t thread = pmath_thread_get_current();
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  while(!pmath_thread_aborting(thread)) {
    pmath_t body;
    pmath_t condition = pmath_evaluate(pmath_expr_get_item(expr, 1));
    
    if(!pmath_same(condition, pmath_System_True)) {
      if(!pmath_same(condition, pmath_System_False)) {
        pmath_message(PMATH_NULL, "cond", 1, pmath_ref(condition));
      }
      pmath_unref(condition);
      break;
    }
    pmath_unref(condition);
    
    body = pmath_expr_get_item(expr, 2);
    if(_pmath_run(&body)) {
      pmath_unref(expr);
      return body;
    }
    
    pmath_unref(body);
  }
  
  pmath_unref(expr);
  return PMATH_NULL;
}
