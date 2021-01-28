#include <pmath-util/emit-and-gather.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_ConditionalExpression;

PMATH_PRIVATE pmath_t builtin_conditionalexpression(pmath_expr_t expr) {
  pmath_t value, test;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  value = pmath_expr_get_item(expr, 1);
  test  = pmath_expr_get_item(expr, 2);
  
  if(pmath_same(test, PMATH_SYMBOL_TRUE)) {
    pmath_unref(test);
    pmath_unref(expr);
    return value;
  }
  
  if( pmath_same(test, PMATH_SYMBOL_FALSE) ||
      pmath_same(test, PMATH_SYMBOL_UNDEFINED))
  {
    pmath_unref(value);
    pmath_unref(test);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }
  
  if(pmath_is_expr_of_len(value, pmath_System_ConditionalExpression, 2)) {
    pmath_t inner_test = pmath_expr_get_item(value, 2);
    
    pmath_unref(expr);
    test = pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_AND), 2,
             test,
             inner_test);
    value = pmath_expr_set_item(value, 2, test);
    return value;
  }
  
  pmath_unref(value);
  pmath_unref(test);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_operate_conditionalexpression(pmath_expr_t expr) {
  pmath_t item = pmath_expr_get_item(expr, 0);
  size_t i;
  
  if( !pmath_is_symbol(item) ||
      !(pmath_symbol_get_attributes(item) & (PMATH_SYMBOL_ATTRIBUTE_DEFINITEFUNCTION | PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION)))
  {
    pmath_unref(item);
    return expr;
  }
  
  pmath_unref(item);
  pmath_gather_begin(PMATH_NULL);
  for(i = 1; i <= pmath_expr_length(expr); ++i) {
    item = pmath_expr_get_item(expr, i);
    
    if(pmath_is_expr_of_len(item, pmath_System_ConditionalExpression, 2)) {
      pmath_emit(pmath_expr_get_item(item, 2), PMATH_NULL);
      expr = pmath_expr_set_item(expr, i, pmath_expr_get_item(item, 1));
    }
    
    pmath_unref(item);
  }
  item = pmath_gather_end();
  
  if(pmath_expr_length(item) == 0) {
    pmath_unref(item);
    return expr;
  }
  
  if(pmath_expr_length(item) > 1) {
    item = pmath_expr_set_item(item, 0, pmath_ref(PMATH_SYMBOL_AND));
  }
  else {
    pmath_t tmp = pmath_expr_get_item(item, 1);
    pmath_unref(item);
    item = tmp;
  }
  
  expr = pmath_expr_new_extended(
           pmath_ref(pmath_System_ConditionalExpression), 2,
           expr,
           item);
           
  return expr;
}

PMATH_PRIVATE pmath_t builtin_operate_undefined(pmath_expr_t expr) {
  pmath_t head = pmath_expr_get_item(expr, 0);
  
  if( pmath_is_symbol(head) &&
      0 != (pmath_symbol_get_attributes(head) & (PMATH_SYMBOL_ATTRIBUTE_DEFINITEFUNCTION | PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION)))
  {
    pmath_unref(head);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }
  
  pmath_unref(head);
  return expr;
}
