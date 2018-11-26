#include <pmath-core/expressions.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/concurrency/threadpool-private.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/parallel-private.h>


PMATH_PRIVATE pmath_t builtin_abort(pmath_expr_t expr) {
  /* Abort()
     Abort(task)
  
     messages:
       General::notask
   */
  pmath_symbol_t  sym;
  pmath_custom_t  custom_task;
  pmath_thread_t  thread;
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen == 0) {
    pmath_unref(expr);
    pmath_throw(PMATH_ABORT_EXCEPTION);
    return PMATH_NULL;
  }
  
  if(exprlen > 1) {
    pmath_message_argxxx(exprlen, 0, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "nothread", 1, sym);
    return expr;
  }
  
  custom_task = pmath_symbol_get_value(sym);
  if( !pmath_is_custom(custom_task) ||
      !pmath_custom_has_destructor(custom_task, _pmath_custom_task_destroy))
  {
    pmath_unref(custom_task);
    pmath_message(PMATH_NULL, "nothread", 1, sym);
    return expr;
  }
  
  thread = _pmath_task_get_thread(
             (pmath_task_t)pmath_custom_get_data(custom_task));
             
  _pmath_thread_throw(thread, PMATH_ABORT_EXCEPTION);
  
  pmath_unref(custom_task);
  pmath_unref(sym);
  pmath_unref(expr);
  return PMATH_NULL;
}
