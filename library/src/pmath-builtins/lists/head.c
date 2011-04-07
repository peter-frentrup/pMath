#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>


PMATH_PRIVATE pmath_t _pmath_object_head(pmath_t obj){ // obj wont be freed
  if(pmath_is_double(obj))
    return pmath_ref(PMATH_SYMBOL_REAL);
  
  if(pmath_is_int32(obj))
    return pmath_ref(PMATH_SYMBOL_INTEGER);
  
  if(pmath_is_ministr(obj))
    return pmath_ref(PMATH_SYMBOL_STRING);
  
  if(!pmath_is_pointer(obj))
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  
  if(!PMATH_AS_PTR(obj))
    return PMATH_NULL;
  
  switch(PMATH_AS_PTR(obj)->type_shift){
    case PMATH_TYPE_SHIFT_MP_FLOAT:  return pmath_ref(PMATH_SYMBOL_REAL);
    case PMATH_TYPE_SHIFT_MP_INT:    return pmath_ref(PMATH_SYMBOL_INTEGER);
    case PMATH_TYPE_SHIFT_QUOTIENT:  return pmath_ref(PMATH_SYMBOL_RATIONAL);
    case PMATH_TYPE_SHIFT_BIGSTRING: return pmath_ref(PMATH_SYMBOL_STRING);
    case PMATH_TYPE_SHIFT_SYMBOL:    return pmath_ref(PMATH_SYMBOL_SYMBOL);
    
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
    case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: 
      return pmath_expr_get_item(obj, 0);
  }
  
  return pmath_ref(PMATH_SYMBOL_UNDEFINED);
}

PMATH_PRIVATE pmath_t builtin_head(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  expr = _pmath_object_head(obj);
  pmath_unref(obj);
  return expr;
}
