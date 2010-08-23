#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <assert.h>

#include <pmath-util/approximate.h>
#include <pmath-util/messages.h>

#include <pmath-core/numbers-private.h>

#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE
pmath_t builtin_accuracy(pmath_expr_t expr){
  double acc;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  acc = pmath_accuracy(expr);
  
  if(isfinite(acc))
    return pmath_float_new_d(acc * LOG10_2);
  
  return pmath_ref(_pmath_object_infinity);
}

PMATH_PRIVATE
pmath_t builtin_precision(pmath_expr_t expr){
  double prec;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  prec = pmath_precision(expr);
  
  if(isfinite(prec))
    return pmath_float_new_d(prec * LOG10_2);
  
  if(prec < 0)
    return pmath_ref(PMATH_SYMBOL_MACHINEPRECISION);
    
  return pmath_ref(_pmath_object_infinity);
  
//  mp_prec_t prec;
//
//  if(pmath_expr_length(expr) != 1){
//    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
//    return expr;
//  }
//  
//  prec = precision(expr);
//  
//  if(prec == 0)
//    return pmath_ref(PMATH_SYMBOL_MACHINEPRECISION);
//  
//  if(prec > MPFR_PREC_MAX)
//    return pmath_ref(_pmath_object_infinity);
//  
//  return pmath_float_new_d(prec * LOG10_2);
}


PMATH_PRIVATE 
pmath_t builtin_setaccuracy(pmath_expr_t expr){
  pmath_t acc_obj;
  double acc;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  acc_obj = pmath_expr_get_item(expr, 2);
  if(!pmath_instance_of(acc_obj, PMATH_TYPE_NUMBER)){
    pmath_message(NULL, "invacc", 1, acc_obj);
    return expr;
  }
  
  acc = LOG2_10 * pmath_number_get_d(acc_obj);
  pmath_unref(acc_obj);
  
  if(fabs(acc) > PMATH_MP_PREC_MAX){
    pmath_unref(expr);
    pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
    return pmath_ref(_pmath_object_overflow);
  }
  
  acc_obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  return pmath_set_accuracy(acc_obj, acc);
}

PMATH_PRIVATE 
pmath_t builtin_setprecision(pmath_expr_t expr){
  pmath_t prec_obj;
  double prec;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  prec_obj = pmath_expr_get_item(expr, 2);
  
  if(prec_obj == PMATH_SYMBOL_MACHINEPRECISION){
    prec = -HUGE_VAL;
  }
  else{
    if(!pmath_instance_of(prec_obj, PMATH_TYPE_NUMBER)){
      pmath_message(NULL, "invprec", 1, prec_obj);
      return expr;
    }
    
    prec = LOG2_10 * pmath_number_get_d(prec_obj);
    
    if(fabs(prec) > PMATH_MP_PREC_MAX){
      pmath_unref(prec_obj);
      pmath_unref(expr);
      pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
      return pmath_ref(_pmath_object_overflow);
    }
  }
  
  pmath_unref(prec_obj);
  
  prec_obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  return pmath_set_precision(prec_obj, prec);
}


PMATH_PRIVATE 
pmath_t builtin_assign_maxextraprecision(pmath_expr_t expr){
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
  
  if(lhs != PMATH_SYMBOL_MAXEXTRAPRECISION){
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(rhs == PMATH_UNDEFINED){ // Unassign({...})
    pmath_max_extra_precision = 50 * LOG2_10;
    
    pmath_unref(tag);
    pmath_unref(rhs);
    pmath_unref(expr);
    
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_ASSIGNDELAYED), 2,
      lhs,
      pmath_integer_new_si(50));
  }
  
  if(pmath_equals(rhs, _pmath_object_infinity)){
    pmath_max_extra_precision = HUGE_VAL;
    
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(pmath_instance_of(rhs, PMATH_TYPE_NUMBER)){
    double d = pmath_number_get_d(rhs);
    
    if(d > 0){
      pmath_max_extra_precision = d * LOG2_10;
      
      pmath_unref(tag);
      pmath_unref(lhs);
      pmath_unref(rhs);
      return expr;
    }
  }
  
  pmath_message(PMATH_SYMBOL_MAXEXTRAPRECISION, "meprecset", 1, rhs);
  pmath_unref(tag);
  pmath_unref(lhs);
  
  lhs = pmath_expr_get_item(expr, 0);
  pmath_unref(lhs);
  pmath_unref(expr);
  if(lhs == PMATH_SYMBOL_ASSIGNDELAYED
  || lhs == PMATH_SYMBOL_TAGASSIGNDELAYED)
    return NULL;
  
  return pmath_ref(PMATH_SYMBOL_FAILED);
}
