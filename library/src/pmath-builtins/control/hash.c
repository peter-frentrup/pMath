#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

PMATH_PRIVATE pmath_t builtin_hash(pmath_expr_t expr){
  pmath_t  obj;
  unsigned int    hash;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  hash = pmath_hash(obj);
  pmath_unref(obj);

  return pmath_integer_new_ui(hash);
}
