#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_finally(pmath_expr_t expr){
  pmath_thread_t   current_thread;
  pmath_t   exception, fst;
  intptr_t         old_ignore_older_aborts;

  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  current_thread = pmath_thread_get_current();
  if(!current_thread){
    pmath_unref(expr);
    return PMATH_NULL;
  }

  fst = pmath_evaluate(pmath_expr_get_item(expr, 1));

  old_ignore_older_aborts = current_thread->ignore_older_aborts;
  current_thread->ignore_older_aborts = 1 + pmath_atomic_read_aquire(&_pmath_abort_timer);
  exception = _pmath_thread_catch(current_thread);
  
  pmath_unref(pmath_evaluate(pmath_expr_get_item(expr, 2)));

  pmath_throw(exception);
  current_thread->ignore_older_aborts = old_ignore_older_aborts;

  pmath_unref(expr);
  return fst;
}
