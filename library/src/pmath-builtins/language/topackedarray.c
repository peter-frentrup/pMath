#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_Automatic;
extern pmath_symbol_t pmath_System_Integer;
extern pmath_symbol_t pmath_System_Real;

PMATH_PRIVATE pmath_t builtin_developer_topackedarray(pmath_expr_t expr) {
// Developer`ToPackedArray(expr)
// Developer`ToPackedArray(expr, type)
// 
// TODO: Allow conversion to more restrictive type (Real -> Integer) and
//       specifying tolerance setting for conversion
//
// 1) Integer
//  pmath> intList:= {0, 8, 15, -42}
//         {0, 8, 15, -42}
//  pmath> packedIntList:= intList |> Developer`ToPackedArray
//         {0, 8, 15, -42}
//  
//  pmath> intList === packedIntList
//         True
//  
//  pmath> Developer`PackedArrayForm(intList)
//         {0, 8, 15, -42}
//  pmath> Developer`PackedArrayForm(packedIntList)
//         PackedArray(Integer, <<4>>)
//  
// 2) Real
//  pmath> realList:= {0.0, 8.0, 1.5, -4.2}
//         {0.0, 8.0, 1.5, -4.2}
//  pmath> packedRealList:= realList |> Developer`ToPackedArray
//         {0.0, 8.0, 1.5, -4.2}
//  
//  pmath> Developer`PackedArrayForm(realList)
//         {0.0, 8.0, 1.5, -4.2}
//  pmath> Developer`PackedArrayForm(packedRealList)
//         PackedArray(Real, <<4>>)
//  
//  pmath> realList === packedRealList
//         True
//  
// 3) Mixed
//  pmath> mixedList:= {0, 1.0, -4.0, 5.5}
//         {0, 1.0, -4.0, 5.5}
//  pmath> failPackMixedAsInteger:= mixedList |> Developer`ToPackedArray(Integer)
//         {0, 1.0, -4.0, 5.5}
//  pmath> packMixedAsReal:= mixedList |> Developer`ToPackedArray(Real)
//         {0.0, 1.0, -4.0, 5.5}
//  
//  pmath> mixedList === failPackMixedAsInteger
//         True
//  
//  pmath> Developer`PackedArrayForm(mixedList)
//         {0, 1.0, -4.0, 5.5}
//  pmath> Developer`PackedArrayForm(failPackMixedAsInteger)
//         {0, 1.0, -4.0, 5.5}
//  pmath> Developer`PackedArrayForm(packMixedAsReal)
//         PackedArray(Real, <<4>>)
//
//  pmath> Hash(Numericalize(mixedList)) === Hash(packMixedAsReal)
//         True
//  pmath> Numericalize(mixedList) === packMixedAsReal
//         True
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
