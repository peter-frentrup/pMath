#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_first(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    if(pmath_expr_length(obj) > 0){
      pmath_unref(expr);
      expr = pmath_expr_get_item(obj, 1);
      pmath_unref(obj);
      return expr;
    }
    
    pmath_message(NULL, "first", 1, obj);
    return expr;
  }
  
  pmath_message(NULL, "nexprat", 2, pmath_integer_new_si(1), pmath_ref(expr));
  
  pmath_unref(obj);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_last(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    if(pmath_expr_length(obj) > 0){
      pmath_unref(expr);
      expr = pmath_expr_get_item(obj, pmath_expr_length(obj));
      pmath_unref(obj);
      return expr;
    }
    
    pmath_message(NULL, "nolast", 1, obj);
    return expr;
  }
  
  pmath_message(NULL, "nexprat", 2, pmath_integer_new_si(1), pmath_ref(expr));
  
  pmath_unref(obj);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_most(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    if(pmath_expr_length(obj) > 0){
      pmath_unref(expr);
      expr = pmath_expr_get_item_range(
        obj, 1, pmath_expr_length(obj) - 1);
      pmath_unref(obj);
      return expr;
    }
    
    pmath_message(NULL, "norest", 1, obj);
    return expr;
  }
  
  pmath_message(NULL, "nexprat", 2, pmath_integer_new_si(1), pmath_ref(expr));
  
  pmath_unref(obj);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_rest(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    if(pmath_expr_length(obj) > 0){
      pmath_unref(expr);
      expr = pmath_expr_get_item_range(
        obj, 2, pmath_expr_length(obj));
      pmath_unref(obj);
      return expr;
    }
    
    pmath_message(NULL, "norest", 1, obj);
    return expr;
  }
  
  pmath_message(NULL, "nexprat", 2, pmath_integer_new_si(1), pmath_ref(expr));
  
  pmath_unref(obj);
  return expr;
}
