#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_developer_topackedarray(pmath_expr_t expr) {
/* Developer`ToPackedArray(expr)
   Developer`ToPackedArray(expr, type)
   
   TODO: allow conversion to more restrictive type (Real -> Integer) and
         specifying tolerance setting for conversion
 */
  pmath_t list;
  pmath_packed_type_t expected_type = PMATH_PACKED_INT32;
  
  if(pmath_expr_length(expr) < 1 || pmath_expr_length(expr) > 2){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
  
  if(pmath_expr_length(expr) == 2) {
    pmath_t type = pmath_expr_get_item(expr, 2);
    pmath_unref(type);
    
    if(pmath_same(type, PMATH_SYMBOL_INTEGER))
      expected_type = PMATH_PACKED_INT32;
    else if(pmath_same(type, PMATH_SYMBOL_REAL))
      expected_type = PMATH_PACKED_DOUBLE;
    else
      return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(!pmath_is_expr(list))
    return list;
  
  list = _pmath_expr_pack_array(list, expected_type);
  
  return list;
}
