#include <pmath-core/expressions.h>

#include <pmath-util/concurrency/threads-private.h>


pmath_t builtin_internal_abortmessage(pmath_expr_t expr) {
  _pmath_abort_message(pmath_expr_get_item(expr, 1));
  
  pmath_unref(expr);
  return PMATH_NULL;
}
