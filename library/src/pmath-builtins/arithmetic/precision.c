#include <pmath-core/numbers-private.h>
#include <pmath-core/intervals-private.h>
#include <pmath-core/packed-arrays.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/approximate.h>
#include <pmath-util/debug.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
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

static pmath_bool_t check_convert_to_precision(pmath_t obj, double *result) { // obj will be freed
  pmath_t tmp;
  
  if(_pmath_to_precision(obj, result)) {
    pmath_unref(obj);
    return TRUE;
  }
  
  tmp = pmath_set_precision(pmath_ref(obj), -HUGE_VAL);
  if(!_pmath_to_precision(tmp, result)) {
    pmath_unref(tmp);
    pmath_message(PMATH_NULL, "invprec", 1, obj);
    return FALSE;
  }
  pmath_unref(tmp);
  
  if(isfinite(*result)) {
    pmath_thread_t me = pmath_thread_get_current();
    if(me) {
      if(*result > me->max_precision) {
        pmath_message(
          PMATH_NULL, "preclg", 2,
          pmath_ref(obj),
          _pmath_from_precision(me->max_precision));
        *result = me->max_precision;
      }
      else if(*result < me->min_precision) {
        pmath_message(
          PMATH_NULL, "precsm", 2,
          pmath_ref(obj),
          _pmath_from_precision(me->min_precision));
        *result = me->min_precision;
      }
    }
    
    if(*result > PMATH_MP_PREC_MAX)
      *result = PMATH_MP_PREC_MAX;
  }
  
  pmath_unref(obj);
  return TRUE;
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
  pmath_t obj;
  double prec;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  if(!check_convert_to_precision(pmath_expr_get_item(expr, 2), &prec))
    return expr;
  
  obj = pmath_expr_get_item(expr, 1);
  
  pmath_unref(expr);
  return pmath_set_precision(obj, prec);
}

PMATH_PRIVATE pmath_t builtin_assign_setprecision(pmath_expr_t expr) {
  /* SetPrecision(sym, ~prec)::= ...
   */
  struct _pmath_symbol_rules_t *rules;
  pmath_t tag;
  pmath_t lhs;
  pmath_t rhs;
  pmath_t sym;
  pmath_t arg;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
    
  if(pmath_is_expr_of(lhs, PMATH_SYMBOL_SETPRECISION)) {
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

static pmath_t _mpfi_get_representative(mpfi_srcptr ival) {
  pmath_mpfloat_t tmp;
  double d;
  long exp;
  
  if(mpfi_nan_p(ival)) 
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  
  tmp = _pmath_create_mp_float(mpfi_get_prec(ival));
  if(pmath_is_null(tmp)) 
    return PMATH_NULL;
  
  if(!mpfi_bounded_p(ival)) {
    if(mpfr_number_p(&ival->left)) {
      mpfr_set(PMATH_AS_MP_VALUE(tmp), &ival->left, MPFR_RNDN);
      tmp = _pmath_float_exceptions(tmp);
      return pmath_set_precision(tmp, 0);
    }
    
    if(mpfr_number_p(&ival->right)) {
      mpfr_set(PMATH_AS_MP_VALUE(tmp), &ival->right, MPFR_RNDN);
      tmp = _pmath_float_exceptions(tmp);
      return pmath_set_precision(tmp, 0);
    }
    
    mpfr_set_ui(PMATH_AS_MP_VALUE(tmp), 0, MPFR_RNDN);
    return pmath_set_precision(tmp, 0);
  }
  
  mpfi_diam(PMATH_AS_MP_VALUE(tmp), ival);
  
  if(!mpfr_number_p(PMATH_AS_MP_VALUE(tmp))) {
    pmath_unref(tmp);
    return pmath_ref(_pmath_object_overflow);
  }
  
  d = mpfr_get_d_2exp(&exp, PMATH_AS_MP_VALUE(tmp), MPFR_RNDD);
  mpfi_get_fr(PMATH_AS_MP_VALUE(tmp), ival);
  tmp = _pmath_float_exceptions(tmp);
  
  if(d == 0)
    return tmp;
  
  if(exp > 0) {
    mpfr_set_ui(PMATH_AS_MP_VALUE(tmp), 0, MPFR_RNDN);
    return pmath_set_precision(tmp, 0);
  }
  
  return pmath_set_precision(tmp, -exp);
}

static pmath_t approximate_to_finite_precision(pmath_t obj, double precision_goal) {
  double prec = pmath_precision(pmath_ref(obj));
  double maxprec;
  pmath_thread_t me = pmath_thread_get_current();
  
  if(!me)
    return obj;
    
  if(isfinite(prec)) {
    if(prec <= precision_goal)
      return obj;
    
    maxprec = prec + me->max_extra_precision;
  }
  else {
    prec = precision_goal;
    maxprec = precision_goal + me->max_extra_precision;
  }
  
  if(maxprec > me->max_precision)
    maxprec = me->max_precision;
  
  while(!pmath_thread_aborting(me)) {
    pmath_t n_obj = pmath_set_precision(pmath_ref(obj), prec);
    double n_prec = pmath_precision(pmath_ref(n_obj));
    
    if(n_prec >= precision_goal) {
      pmath_unref(obj);
      return n_obj;
    }
    
    pmath_unref(n_obj);
    if(prec < maxprec) {
      prec = 2 * prec;
      if(prec > maxprec)
        prec = maxprec;
      
      continue;
    }
    
    break;
  }
  
  pmath_message(
    PMATH_NULL, "meprec", 2, 
    _pmath_from_precision(me->max_extra_precision), 
    pmath_ref(obj)); // N(obj)
  return obj;
}

PMATH_PRIVATE
pmath_t builtin_approximate(pmath_expr_t expr) {
/* N(x)             = N(x, MachinePrecision)
   N(x, precision)
 */
  size_t exprlen = pmath_expr_length(expr);
  pmath_t x;
  double prec;
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
  
  if(exprlen == 2) {
    if(!check_convert_to_precision(pmath_expr_get_item(expr, 2), &prec))
      return expr;
  }
  else
    prec = -HUGE_VAL;
  
  x = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(prec == -HUGE_VAL)
    return pmath_set_precision(x, prec);
  
  if(isfinite(prec))
    return approximate_to_finite_precision(x, prec);
    
  return x;
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
