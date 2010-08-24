#include <pmath-core/numbers-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/number-theory-private.h>

PMATH_PRIVATE
pmath_bool_t _pmath_is_infinite(pmath_t obj){
  pmath_bool_t result;
  pmath_t item;
  
  if(!pmath_instance_of(obj, PMATH_TYPE_EXPRESSION))
    return FALSE;
  
  item = pmath_expr_get_item(obj, 0);
  pmath_unref(item);
  
  if(item != PMATH_SYMBOL_DIRECTEDINFINITY)
    return FALSE;
  
  if(pmath_expr_length(obj) == 0)
    return TRUE;
  
  if(pmath_expr_length(obj) > 1)
    return FALSE;
  
  item = pmath_expr_get_item(obj, 1);
  result = _pmath_is_numeric(item);
  pmath_unref(item);
  
  return result;
}

PMATH_PRIVATE pmath_t _pmath_directed_infinity_direction(
  pmath_t obj
){
  pmath_t head;
  if(!pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)
  || pmath_expr_length(obj) > 1)
    return NULL;

  head = pmath_expr_get_item((pmath_expr_t)obj, 0);
  pmath_unref(head);
  if(head != PMATH_SYMBOL_DIRECTEDINFINITY)
    return NULL;

  if(pmath_expr_length(obj) == 0)
    return pmath_integer_new_si(0);

  return pmath_expr_get_item((pmath_expr_t)obj, 1);
}

PMATH_PRIVATE pmath_t builtin_operate_indeterminate(pmath_expr_t expr){
  pmath_t head = pmath_expr_get_item(expr, 0);
  
  if(pmath_instance_of(head, PMATH_TYPE_SYMBOL)
  && pmath_symbol_get_attributes(head) & PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION){
    pmath_unref(head);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_INDETERMINATE);
  }
  
  pmath_unref(head);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_directedinfinity(pmath_expr_t expr){
  pmath_t item, sign;
  size_t len = pmath_expr_length(expr);
  
  if(len == 0)
    return expr;
    
  if(len > 1){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 1);
    return expr;
  }

  item = pmath_expr_get_item(expr, 1);
  if(item == PMATH_SYMBOL_INDETERMINATE
  || (pmath_instance_of(item, PMATH_TYPE_NUMBER)
   && pmath_number_sign((pmath_number_t)item) == 0)){
    pmath_unref(item);
    pmath_unref(expr);
    return pmath_ref(_pmath_object_complex_infinity);
  }

  sign = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_SIGN), 1,
      pmath_ref(item)));

  if(pmath_equals(sign, item)){
    pmath_unref(sign);
    pmath_unref(item);
    return expr;
  }
  pmath_unref(item);
  item = sign;

  if(pmath_instance_of(sign, PMATH_TYPE_EXPRESSION)
  && pmath_expr_length(sign) == 1){
    pmath_t signhead = pmath_expr_get_item((pmath_expr_t)sign, 0);
    pmath_unref(signhead);
    if(signhead == PMATH_SYMBOL_SIGN){
      item = pmath_expr_get_item((pmath_expr_t)sign, 1);
      pmath_unref(sign);
    }
  }

  return pmath_expr_set_item(expr, 1, item);
}
