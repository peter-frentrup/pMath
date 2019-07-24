#include <pmath-core/expressions.h>

#include <pmath-util/compression.h>
#include <pmath-util/messages.h>


PMATH_PRIVATE pmath_t builtin_compress(pmath_expr_t expr) {
  pmath_string_t result;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  result = pmath_compress_to_string(pmath_expr_get_item(expr, 1));
  if(pmath_is_null(result))
    return expr;
  
  pmath_unref(expr);
  return result;
}

PMATH_PRIVATE pmath_t builtin_uncompress(pmath_expr_t expr) {
/* Uncompress("data")
   Uncompress("data", head)
 */
  pmath_t obj;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(obj)) {
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    pmath_unref(obj);
    return expr;
  }
  
  obj = pmath_decompress_from_string(obj);
  if(pmath_same(obj, PMATH_UNDEFINED)) 
    return expr;
  
  if(exprlen == 2){
    pmath_t head = pmath_expr_get_item(expr, 2);
    obj = pmath_expr_new_extended(head, 1, obj);
  }
  
  pmath_unref(expr);
  return obj;
}
