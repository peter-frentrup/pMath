#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

/* see scripts/auto/core/linalg.pmath */

pmath_t builtin_transpose(pmath_expr_t expr){
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  return expr;
}
