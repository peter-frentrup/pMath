#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE
pmath_bool_t _pmath_contains_symbol(
  pmath_t        obj, // wont be freed
  pmath_symbol_t sub  // wont be freed
){
  size_t i;
  pmath_t item;
  pmath_bool_t test;
  
  if(obj == sub)
    return TRUE;
  
  if(!pmath_instance_of(obj, PMATH_TYPE_EXPRESSION))
    return FALSE;
  
  i = pmath_expr_length(obj);
  while(i > 0){
    item = pmath_expr_get_item(obj, i);
    test = _pmath_contains_symbol(item, sub);
    pmath_unref(item);
    if(test)
      return TRUE;
    --i;
  }
  
  item = pmath_expr_get_item(obj, 0);
  test = _pmath_contains_symbol(item, sub);
  pmath_unref(item);
  
  return test;
}

PMATH_PRIVATE pmath_t builtin_length(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    size_t len = pmath_expr_length((pmath_expr_t)obj);
    pmath_unref(expr);
    pmath_unref(obj);
    return pmath_integer_new_size(len);
  }
  
  if(pmath_instance_of(obj, PMATH_TYPE_STRING)){
    size_t len = pmath_string_length((pmath_string_t)obj);
    pmath_unref(expr);
    pmath_unref(obj);
    return pmath_integer_new_size(len);
  }
  
  pmath_unref(obj);
  pmath_unref(expr);
  return pmath_integer_new_ui(0);
}
