#include <pmath-builtins/arithmetic-private.h>

#include <pmath-core/intervals.h>

#include <pmath-util/messages.h>


PMATH_PRIVATE pmath_t builtin_internal_realinterval(pmath_expr_t expr) {
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 2 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  return pmath_interval_from_expr(expr);
}
