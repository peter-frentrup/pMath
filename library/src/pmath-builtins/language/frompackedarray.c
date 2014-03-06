#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_developer_frompackedarray(pmath_expr_t expr) {
/* Developer`FromPackedArray(expr)
 */
  pmath_t array;

  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  array = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(!pmath_is_packed_array(array))
    return array;
  
  array = _pmath_expr_unpack_array(array, TRUE);
  
  return array;
}
