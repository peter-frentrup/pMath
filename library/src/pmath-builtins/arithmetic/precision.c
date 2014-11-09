#include <pmath-core/numbers-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/approximate.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/number-theory-private.h>


PMATH_PRIVATE pmath_bool_t _pmath_to_precision(
  pmath_t  obj, // wont be freed
  double  *result
) {
  if(pmath_same(obj, PMATH_SYMBOL_MACHINEPRECISION)) {
    *result = -HUGE_VAL;
    return TRUE;
  }
  
  if(pmath_is_number(obj)) {
    *result = LOG2_10 * pmath_number_get_d(obj);
    return isfinite(*result);
  }
  
  if(pmath_equals(obj, _pmath_object_pos_infinity)) {
    *result = HUGE_VAL;
    return TRUE;
  }
  
  return FALSE;
}

PMATH_PRIVATE pmath_t _pmath_from_precision(double prec_bits) {
  if(prec_bits == -HUGE_VAL)
    return pmath_ref(PMATH_SYMBOL_MACHINEPRECISION);
    
  if(prec_bits == HUGE_VAL)
    return pmath_ref(_pmath_object_pos_infinity);
    
  prec_bits *= LOG10_2;
  if(isfinite(prec_bits))
    return PMATH_FROM_DOUBLE(prec_bits);
    
  return pmath_ref(PMATH_SYMBOL_FAILED);
}

PMATH_PRIVATE
pmath_t builtin_precision(pmath_expr_t expr) {
  double prec;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  prec = pmath_precision(expr);
  
  if(isfinite(prec))
    return PMATH_FROM_DOUBLE(prec * LOG10_2);
    
  if(prec < 0)
    return pmath_ref(PMATH_SYMBOL_MACHINEPRECISION);
    
  return pmath_ref(_pmath_object_pos_infinity);
}

PMATH_PRIVATE
pmath_t builtin_setprecision(pmath_expr_t expr) {
  /* SetPrecision(                 obj, p)
     Internal`SetPrecisionInterval(obj, p)
   */
  pmath_t prec_obj;
  double prec;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  prec_obj = pmath_expr_get_item(expr, 2);
  
  if(pmath_same(prec_obj, PMATH_SYMBOL_MACHINEPRECISION)) {
    prec = -HUGE_VAL;
  }
  else if(_pmath_number_class(prec_obj) & PMATH_CLASS_POSINF) {
    prec = HUGE_VAL;
  }
  else {
    if(!pmath_is_number(prec_obj)) {
      prec_obj = pmath_set_precision(prec_obj, -HUGE_VAL);
      
      if(!pmath_is_number(prec_obj)) {
        pmath_unref(prec_obj);
        prec_obj = pmath_expr_get_item(expr, 2);
        pmath_message(PMATH_NULL, "invprec", 1, prec_obj);
        return expr;
      }
    }
    
    prec = LOG2_10 * pmath_number_get_d(prec_obj);
    
    if(fabs(prec) > PMATH_MP_PREC_MAX) {
      pmath_unref(prec_obj);
      pmath_unref(expr);
      pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
      return pmath_ref(_pmath_object_overflow);
    }
  }
  
  pmath_unref(prec_obj);
  
  prec_obj = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_expr_of(expr, PMATH_SYMBOL_INTERNAL_SETPRECISIONINTERVAL)) {
    if(prec == HUGE_VAL) {
      pmath_unref(prec_obj);
      prec_obj = pmath_expr_get_item(expr, 2);
      pmath_message(PMATH_NULL, "invprec", 1, prec_obj);
      return expr;
    }
    
    pmath_unref(expr);
    return pmath_set_precision_interval(prec_obj, prec);
  }
  
  pmath_unref(expr);
  return pmath_set_precision(prec_obj, prec);
}

PMATH_PRIVATE pmath_t builtin_assign_setprecision(pmath_expr_t expr) {
  /* SetPrecision(                 sym, ~prec)::= ...
     Internal`SetPrecisionInterval(sym, ~prec)::= ...
   */
  struct _pmath_symbol_rules_t *rules;
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_t sym;
  pmath_t arg;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
    
  if(pmath_is_expr_of(lhs, PMATH_SYMBOL_SETPRECISION) ||
      pmath_is_expr_of(lhs, PMATH_SYMBOL_INTERNAL_SETPRECISIONINTERVAL))
  {
    if(pmath_expr_length(lhs) != 2) {
      pmath_unref(tag);
      pmath_unref(lhs);
      pmath_unref(rhs);
      return expr;
    }
  }
  else {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  arg = pmath_expr_get_item(lhs, 1);
  sym = _pmath_topmost_symbol(arg);
  pmath_unref(arg);
  
  if(!pmath_same(tag, PMATH_UNDEFINED) &&
      !pmath_same(tag, sym))
  {
    pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
    
    pmath_unref(expr);
    if(pmath_same(rhs, PMATH_UNDEFINED))
      return pmath_ref(PMATH_SYMBOL_FAILED);
    return rhs;
  }
  
  pmath_unref(tag);
  pmath_unref(expr);
  
  rules = _pmath_symbol_get_rules(sym, RULES_WRITE);
  pmath_unref(sym);
  
  if(!rules) {
    pmath_unref(lhs);
    pmath_unref(rhs);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  _pmath_rulecache_change(&rules->approx_rules, lhs, rhs);
  
  return PMATH_NULL;
}


PMATH_PRIVATE
pmath_t builtin_assign_maxextraprecision(pmath_expr_t expr) {
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_thread_t me = pmath_thread_get_current();
  
  if(!me)
    return expr;
    
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
    
  if(!pmath_same(lhs, PMATH_SYMBOL_MAXEXTRAPRECISION)) {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(pmath_same(rhs, PMATH_UNDEFINED)) { // $MaxExtraPrecision:= .
    me->max_extra_precision = 50 * LOG2_10;
    
    pmath_unref(tag);
    pmath_unref(rhs);
    pmath_unref(expr);
    
    return pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_ASSIGNDELAYED), 2,
             lhs,
             PMATH_FROM_INT32(50));
  }
  
  if(pmath_equals(rhs, _pmath_object_pos_infinity)) {
    me->max_extra_precision = HUGE_VAL;
    
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(pmath_is_number(rhs)) {
    double d = pmath_number_get_d(rhs);
    
    if(d > 0) {
      me->max_extra_precision = d * LOG2_10;
      
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
  if( pmath_same(lhs, PMATH_SYMBOL_ASSIGNDELAYED) ||
      pmath_same(lhs, PMATH_SYMBOL_TAGASSIGNDELAYED))
  {
    return PMATH_NULL;
  }
  
  return pmath_ref(PMATH_SYMBOL_FAILED);
}
