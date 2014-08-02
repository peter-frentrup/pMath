#include <pmath-util/approximate.h>

#include <pmath-core/symbols-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/lists-private.h>


static pmath_t prec_to_obj(double binprec) {
  if(binprec == -HUGE_VAL)
    return pmath_ref(PMATH_SYMBOL_MACHINEPRECISION);
    
  if(binprec == HUGE_VAL)
    return pmath_ref(_pmath_object_pos_infinity);
    
  return PMATH_FROM_DOUBLE(LOG10_2 * binprec);
}

static pmath_t precacc_to_obj(double binprec, double binacc) {
  return pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_LIST), 2,
           prec_to_obj(binprec),
           prec_to_obj(binacc));
}

PMATH_PRIVATE pmath_t _pmath_approximate_step(
  pmath_t obj, // will be freed
  double  prec,
  double  acc_unused
) {
  pmath_symbol_t sym;
  
  if(pmath_is_number(obj)) {
    double oldprec = pmath_precision(pmath_ref(obj));
    
    if(oldprec < prec)
      return obj;
      
    return pmath_set_precision(obj, prec);
  }
  
  sym = _pmath_topmost_symbol(obj);
  if(!pmath_is_null(sym)) {
    struct _pmath_symbol_rules_t  *rules;
    pmath_t result;
    
    rules = _pmath_symbol_get_rules(sym, RULES_READ);
    
    if(rules) {
      result = pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_N), 2,
                 pmath_ref(obj),
                 precacc_to_obj(prec, acc_unused));
                 
      if(_pmath_rulecache_find(&rules->approx_rules, &result)) {
        pmath_unref(sym);
        pmath_unref(obj);
        return pmath_evaluate(result);
      }
      
      pmath_unref(result);
    }
    
    result = pmath_ref(obj);
    if(_pmath_run_approx_code(sym, &result, prec, acc_unused)) {
      if(!pmath_equals(result, obj)) {
        pmath_unref(obj);
        pmath_unref(sym);
        return result;
      }
    }
    
    pmath_unref(sym);
    pmath_unref(result);
  }
  
  if(pmath_is_expr(obj)) {
    size_t len;
    pmath_symbol_attributes_t attr;
    
    if(pmath_is_packed_array(obj)) {
      switch(pmath_packed_array_get_element_type(obj)) {
        case PMATH_PACKED_INT32:
          if(prec == -HUGE_VAL)
            return _pmath_expr_pack_array(obj, PMATH_PACKED_DOUBLE);
            
          if(prec == HUGE_VAL)
            return obj;
            
          break;
          
        case PMATH_PACKED_DOUBLE:
          return obj;
      }
    }
    
    len = pmath_expr_length(obj);
    attr = 0;
    sym = pmath_expr_get_item(obj, 0);
    if(pmath_is_symbol(sym)) {
      attr = pmath_symbol_get_attributes(sym);
      pmath_unref(sym);
    }
    else {
      obj = pmath_expr_set_item(
              obj, 0,
              _pmath_approximate_step(sym, prec, acc_unused));
    }
    
    len = pmath_expr_length(obj);
    
    if(len > 0) {
      if(!(attr & PMATH_SYMBOL_ATTRIBUTE_NHOLDFIRST)) {
        pmath_t first = pmath_expr_extract_item(obj, 1);
        obj = pmath_expr_set_item(obj, 1, _pmath_approximate_step(first, prec, acc_unused));
      }
      
      if(!(attr & PMATH_SYMBOL_ATTRIBUTE_NHOLDREST)) {
        size_t i;
        for(i = 2; i <= len; ++i) {
          pmath_t item = pmath_expr_extract_item(obj, i);
          obj = pmath_expr_set_item(obj, i, _pmath_approximate_step(item, prec, acc_unused));
        }
      }
    }
    
    if(!(attr & PMATH_SYMBOL_ATTRIBUTE_NHOLDALL))
      return pmath_evaluate(obj);
  }
  
  return obj;
}

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

static pmath_bool_t obj_to_accprec(
  pmath_t  obj, // will be freed
  double  *acc,
  double  *prec
) {
  *acc = HUGE_VAL;
  if(!_pmath_to_precision(obj, prec)) {
    if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_LIST, 2)) {
      pmath_t prec_obj = pmath_expr_get_item(obj, 1);
      pmath_t acc_obj  = pmath_expr_get_item(obj, 2);
      
      pmath_unref(obj);
      
      if(!_pmath_to_precision(prec_obj, prec)) {
        pmath_message(PMATH_NULL, "invprec", 1, prec_obj);
        pmath_unref(acc_obj);
        return FALSE;
      }
      
      if(!_pmath_to_precision(acc_obj, acc)) {
        pmath_message(PMATH_NULL, "invacc", 1, acc_obj);
        pmath_unref(prec_obj);
        return FALSE;
      }
      
      pmath_unref(prec_obj);
      pmath_unref(acc_obj);
      return TRUE;
    }
    
    pmath_message(PMATH_NULL, "invprec", 1, obj);
    return FALSE;
  }
  
  pmath_unref(obj);
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_approximate(pmath_expr_t expr) {
  /* N(obj)              = N(obj, MachinePrecision)
     N(obj, prec)        = N(obj, {prec, Infinity})
     N(obj, {prec, acc})
  
     Messages
       General::invacc
       General::invprec
   */
  size_t len = pmath_expr_length(expr);
  double prec_goal, acc_goal;
  pmath_t obj;
  
  if(len == 2) {
    if(!obj_to_accprec(pmath_expr_get_item(expr, 2), &acc_goal, &prec_goal))
      return expr;
  }
  else if(len == 1) {
    prec_goal = acc_goal = -HUGE_VAL;
  }
  else {
    pmath_message_argxxx(len, 1, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  return pmath_approximate(obj, prec_goal, acc_goal, NULL);
}

PMATH_PRIVATE pmath_t builtin_assign_approximate(pmath_expr_t expr) {
  struct _pmath_symbol_rules_t *rules;
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_t sym;
  pmath_t arg;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
    
  if( !pmath_is_expr_of(lhs, PMATH_SYMBOL_N) &&
      pmath_expr_length(lhs) != 1            &&
      pmath_expr_length(lhs) != 2)
  {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  if(pmath_expr_length(lhs) == 1) {
    lhs = pmath_expr_append(lhs, 1, precacc_to_obj(-HUGE_VAL, -HUGE_VAL));
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

static pmath_t approx_const_generic(
  double prec, 
  double acc,
  int (*generator)(mpfr_ptr, mpfr_rnd_t),
  double double_value
) {
  pmath_mpfloat_t result;
  mpfr_rnd_t rnd = _pmath_current_rounding_mode();
  
  if(acc == -HUGE_VAL || prec == -HUGE_VAL)
    return PMATH_FROM_DOUBLE(double_value);
    
  if(acc < -PMATH_MP_PREC_MAX) {
    pmath_message(PMATH_SYMBOL_GENERAL, "unfl", 0);
    return pmath_ref(_pmath_object_underflow);
  }
  
  if(prec == HUGE_VAL)
    prec = log2(double_value) + acc;
    
  if(prec > PMATH_MP_PREC_MAX) {
    pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
    return pmath_ref(_pmath_object_overflow);
  }
  
//  if(acc == HUGE_VAL) {
//    acc = prec - log2(double_value); 
//  }
  
  if(prec < MPFR_PREC_MIN)
    prec = MPFR_PREC_MIN;
    
  result = _pmath_create_mp_float((mpfr_prec_t)round(prec));
  if(pmath_is_null(result))
    return PMATH_NULL;
  
  generator(PMATH_AS_MP_VALUE(result), rnd);
  
  mpfr_set_ui(PMATH_AS_MP_VALUE(result), 1, rnd);
  mpfr_exp(PMATH_AS_MP_VALUE(result), PMATH_AS_MP_VALUE(result), rnd);
  return result;
}

static int mpfr_const_exp1(mpfr_ptr rop, mpfr_rnd_t rnd) {
  mpfr_set_ui(rop, 1, rnd);
  return mpfr_exp(rop, rop, rnd);
}

static int mpfr_const_machineprecision(mpfr_ptr rop, mpfr_rnd_t rnd) {
  MPFR_DECL_INIT(two, DBL_MANT_DIG);
  
  mpfr_set_ui(two, 2, rnd);
  mpfr_log10(rop, two, rnd);
  return mpfr_mul_ui(rop, rop, DBL_MANT_DIG, rnd);
}

PMATH_PRIVATE pmath_t builtin_approximate_e(pmath_t obj, double prec, double acc) {
  if(!pmath_same(obj, PMATH_SYMBOL_E))
    return obj;
  
  return approx_const_generic(prec, acc, mpfr_const_exp1, M_E);
}

PMATH_PRIVATE pmath_t builtin_approximate_eulergamma(pmath_t obj, double prec, double acc) {
  if(!pmath_same(obj, PMATH_SYMBOL_EULERGAMMA))
    return obj;
  
  return approx_const_generic(prec, acc, mpfr_const_euler, 0.57721566490153286061);
}

PMATH_PRIVATE pmath_t builtin_approximate_machineprecision(pmath_t obj, double prec, double acc) {
  if(!pmath_same(obj, PMATH_SYMBOL_MACHINEPRECISION))
    return obj;
  
  return approx_const_generic(prec, acc, mpfr_const_machineprecision, (double)LOG10_2 * DBL_MANT_DIG);
}

PMATH_PRIVATE pmath_t builtin_approximate_pi(pmath_t obj, double prec, double acc) {
  if(!pmath_same(obj, PMATH_SYMBOL_PI))
    return obj;
    
  return approx_const_generic(prec, acc, mpfr_const_pi, M_PI);
}
