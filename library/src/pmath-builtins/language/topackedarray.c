#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_developer_topackedarray(pmath_expr_t expr) {
/* Developer`ToPackedArray(expr)
  
   TODO: allow specifying array element type and tolerance setting for complex
   value...
 */
  pmath_t list;

  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(!pmath_is_expr(list)) {
    pmath_message(PMATH_NULL, "noexpr", 1, pmath_ref(list));
    return list;
  }
  
  list = _pmath_expr_pack_array(list);
  if(!pmath_is_packed_array(list)) {
    pmath_message(PMATH_NULL, "nopack", 1, pmath_ref(list));
    return list;
  }
  
  return list;
}
