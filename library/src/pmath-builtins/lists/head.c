#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_Integer;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Rational;
extern pmath_symbol_t pmath_System_Real;
extern pmath_symbol_t pmath_System_String;
extern pmath_symbol_t pmath_System_Symbol;
extern pmath_symbol_t pmath_System_Undefined;

PMATH_PRIVATE pmath_t _pmath_object_head(pmath_t obj) { // obj wont be freed
  if(pmath_is_double(obj))
    return pmath_ref(pmath_System_Real);
    
  if(pmath_is_int32(obj))
    return pmath_ref(pmath_System_Integer);
    
  if(pmath_is_ministr(obj))
    return pmath_ref(pmath_System_String);
    
  if(!pmath_is_pointer(obj))
    return pmath_ref(pmath_System_Undefined);
    
  if(!PMATH_AS_PTR(obj))
    return PMATH_NULL;
    
  switch(PMATH_AS_PTR(obj)->type_shift) {
    case PMATH_TYPE_SHIFT_MP_FLOAT:  return pmath_ref(pmath_System_Real);
    case PMATH_TYPE_SHIFT_MP_INT:    return pmath_ref(pmath_System_Integer);
    case PMATH_TYPE_SHIFT_QUOTIENT:  return pmath_ref(pmath_System_Rational);
    case PMATH_TYPE_SHIFT_BIGSTRING: return pmath_ref(pmath_System_String);
    case PMATH_TYPE_SHIFT_SYMBOL:    return pmath_ref(pmath_System_Symbol);
    
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
    case PMATH_TYPE_SHIFT_CUSTOM_EXPRESSION:
      return pmath_expr_get_item(obj, 0);
    
    case PMATH_TYPE_SHIFT_PACKED_ARRAY:
      return pmath_ref(pmath_System_List);
  }
  
  return pmath_ref(pmath_System_Undefined);
}

PMATH_PRIVATE pmath_t builtin_head(pmath_expr_t expr) {
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  expr = _pmath_object_head(obj);
  pmath_unref(obj);
  return expr;
}
