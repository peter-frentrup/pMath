#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/number-theory-private.h>


PMATH_PRIVATE pmath_t builtin_isheld(pmath_expr_t expr){
  if(pmath_expr_length(expr) != 1)
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    
  return expr;
}

PMATH_PRIVATE pmath_t builtin_call_isheld(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 0);
  if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_ISHELD, 1)){
    pmath_t fn = pmath_evaluate(pmath_expr_get_item(obj, 1));
    pmath_unref(obj);
    
    obj = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    
    if(pmath_is_symbol(fn)){
      pmath_symbol_attributes_t attr = pmath_symbol_get_attributes(fn);
      
      if((attr & PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST)
      || (attr & PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE)){
        return pmath_expr_new_extended(fn, 1, obj);
      }
    }
    
    return pmath_expr_new_extended(
      fn, 1, 
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_UNEVALUATED), 1,
        obj));
  }
  
  pmath_unref(obj);
  
  return expr;
}

PMATH_PRIVATE pmath_t builtin_iseven(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_is_integer(obj)
  && mpz_even_p(((struct _pmath_integer_t*)obj)->value)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE pmath_t builtin_isexactnumber(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_instance_of(obj, PMATH_TYPE_RATIONAL)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  
  if(_pmath_is_nonreal_complex(obj)){
    pmath_t part = pmath_expr_get_item(obj, 1);
    if(pmath_instance_of(obj, PMATH_TYPE_RATIONAL)){
      pmath_unref(part);
      part = pmath_expr_get_item(obj, 2);
      if(pmath_instance_of(obj, PMATH_TYPE_RATIONAL)){
        pmath_unref(part);
        pmath_unref(obj);
        return pmath_ref(PMATH_SYMBOL_TRUE);
      }
    }
    pmath_unref(part);
  }
  
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE pmath_t builtin_isfloat(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_instance_of(obj, PMATH_TYPE_FLOAT)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE pmath_t builtin_isinexactnumber(pmath_expr_t expr){
  pmath_bool_t result;
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  
  result = _pmath_is_inexact(obj);
  
  pmath_unref(expr);
  pmath_unref(obj);
  return pmath_ref(result ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE pmath_t builtin_isinteger(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_is_integer(obj)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE pmath_t builtin_ismachinenumber(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_instance_of(obj, PMATH_TYPE_MACHINE_FLOAT)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  
  if(_pmath_is_nonreal_complex(obj)){
    pmath_t part = pmath_expr_get_item(obj, 1);
    if(pmath_instance_of(obj, PMATH_TYPE_MACHINE_FLOAT)){
      pmath_unref(part);
      part = pmath_expr_get_item(obj, 2);
      if(pmath_instance_of(obj, PMATH_TYPE_MACHINE_FLOAT)){
        pmath_unref(part);
        pmath_unref(obj);
        return pmath_ref(PMATH_SYMBOL_TRUE);
      }
    }
    pmath_unref(part);
  }
  
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE pmath_t builtin_isnumber(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_is_number(obj) || _pmath_is_nonreal_complex(obj)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE pmath_t builtin_isodd(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_is_integer(obj)
  && mpz_odd_p(((struct _pmath_integer_t*)obj)->value)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE pmath_t builtin_ispos_or_isneg(pmath_expr_t expr){
  pmath_t head, obj;
  int clazz, needclazz = 0;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  if(     head == PMATH_SYMBOL_ISNEGATIVE)     needclazz = PMATH_CLASS_NEG;
  else if(head == PMATH_SYMBOL_ISNONNEGATIVE)  needclazz = PMATH_CLASS_POS | PMATH_CLASS_ZERO;
  else if(head == PMATH_SYMBOL_ISNONPOSITIVE)  needclazz = PMATH_CLASS_NEG | PMATH_CLASS_ZERO;
  else if(head == PMATH_SYMBOL_ISPOSITIVE)     needclazz = PMATH_CLASS_POS;
  
  obj = pmath_expr_get_item(expr, 1);
  clazz = _pmath_number_class(obj);
  pmath_unref(obj);
  
  if(clazz & PMATH_CLASS_UNKNOWN)
    return expr;
  
  pmath_unref(expr);
  if(clazz & ~needclazz)
    return pmath_ref(PMATH_SYMBOL_FALSE);
  
  return pmath_ref(PMATH_SYMBOL_TRUE);
}

PMATH_PRIVATE pmath_t builtin_isquotient(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_instance_of(obj, PMATH_TYPE_QUOTIENT)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE pmath_t builtin_isrational(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_instance_of(obj, PMATH_TYPE_RATIONAL)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE pmath_t builtin_isstring(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_is_string(obj)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE pmath_t builtin_issymbol(pmath_expr_t expr){
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);

  if(pmath_is_symbol(obj)){
    pmath_unref(obj);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  pmath_unref(obj);
  return pmath_ref(PMATH_SYMBOL_FALSE);
}
