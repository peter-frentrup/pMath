#include <pmath-builtins/lists-private.h>

#include <pmath-core/expressions-private.h>
#include <pmath-util/messages.h>


PMATH_PRIVATE pmath_t builtin_normal(pmath_expr_t expr) {
// Normal(arg)
//
// Example:
//  pmath> Normal(ByteArray("+vv8/f7/"))
//         {250, 251, 252, 253, 254, 255}
//
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen != 1) {
    pmath_message_argxxx(exprlen, 1, 1);
    return expr;
  }
  
  pmath_t arg = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(pmath_is_pointer_of(arg, PMATH_TYPE_CUSTOM_EXPRESSION)) {
    return _pmath_custom_expr_convert_to_normal((void*)PMATH_AS_PTR(arg));
  }
  
  return arg;
}
