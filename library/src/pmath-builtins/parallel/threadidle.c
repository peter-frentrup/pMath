#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_internal_threadidle(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  pmath_thread_sleep();
  return PMATH_NULL;
}
