#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_Automatic;
extern pmath_symbol_t pmath_System_Integer;
extern pmath_symbol_t pmath_System_Real;

PMATH_PRIVATE pmath_t builtin_developer_topackedarray(pmath_expr_t expr) {
/* Developer`ToPackedArray(expr)
   Developer`ToPackedArray(expr, type)
   
   TODO: allow conversion to more restrictive type (Real -> Integer) and
         specifying tolerance setting for conversion
 */
  pmath_t list;
  pmath_packed_type_t expected_type = 0;
  
  if(pmath_expr_length(expr) < 1 || pmath_expr_length(expr) > 2){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
  
  if(pmath_expr_length(expr) == 2) {
    pmath_t type = pmath_expr_get_item(expr, 2);
    pmath_unref(type);
    
    if(pmath_same(type, pmath_System_Automatic))
      expected_type = 0;
    else if(pmath_same(type, pmath_System_Integer))
      expected_type = PMATH_PACKED_INT32;
    else if(pmath_same(type, pmath_System_Real))
      expected_type = PMATH_PACKED_DOUBLE;
    else
      return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  return pmath_to_packed_array(list, expected_type);
}
