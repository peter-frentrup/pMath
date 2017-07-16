#include <pmath-util/approximate.h>

#include <pmath-core/expressions-private.h>
#include <pmath-core/packed-arrays-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/debug.h>
#include <pmath-util/messages.h>
#include <pmath-util/symbol-values-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/lists-private.h>


PMATH_API
double pmath_precision(pmath_t obj) { // will be freed
// -HUGE_VAL = machine precision
  double prec;
  
  if(pmath_is_double(obj)) {
    pmath_unref(obj);
    return -HUGE_VAL;
  }
  
  if(pmath_is_mpfloat(obj)) {
    slong prec = PMATH_AS_ARB_WORKING_PREC(obj);
    slong acc;
    if(arf_is_zero(arb_midref(PMATH_AS_ARB(obj)))) {
      if(fmpz_cmp_si(MAG_EXPREF(arb_radref(PMATH_AS_ARB(obj))), -ARF_PREC_EXACT) >= 0 &&
         fmpz_cmp_si(MAG_EXPREF(arb_radref(PMATH_AS_ARB(obj))), ARF_PREC_EXACT) <= 0) 
      {
        acc = - fmpz_get_si(MAG_EXPREF(arb_radref(PMATH_AS_ARB(obj))));
      }
      else if(fmpz_sgn(MAG_EXPREF(arb_radref(PMATH_AS_ARB(obj)))) < 0)
        acc = ARF_PREC_EXACT; // small error
      else
        acc = -ARF_PREC_EXACT; // huge error
    }
    else
      acc = arb_rel_accuracy_bits(PMATH_AS_ARB(obj));
    pmath_unref(obj);
    if(prec < acc)
      return (double)prec;
    if(acc < 0)
      return 0; // (double)prec;
    return (double)acc;
  }
  
  if(pmath_is_packed_array(obj)) {
    switch(pmath_packed_array_get_element_type(obj)) {
      case PMATH_PACKED_INT32:
        pmath_unref(obj);
        return HUGE_VAL;
        
      case PMATH_PACKED_DOUBLE:
        pmath_unref(obj);
        return -HUGE_VAL;
    }
  }
  
  prec = HUGE_VAL;
  
  if(pmath_is_expr(obj)) {
    size_t i;
    
    for(i = 0; i <= pmath_expr_length(obj); ++i) {
      double prec2 = pmath_precision(
                       pmath_expr_get_item(obj, i));
                       
      if(prec2 < prec) {
        prec = prec2;
        
        if(prec == -HUGE_VAL)
          break;
      }
    }
  }
  
  pmath_unref(obj);
  return prec;
}

static pmath_t set_infinite_precision_number(pmath_number_t obj) {
  if(pmath_is_double(obj)) {
    pmath_float_t mp = _pmath_create_mp_float_from_d(PMATH_AS_DOUBLE(obj));
    
    if(pmath_is_null(mp))
      return obj;
      
    pmath_unref(obj);
    obj = mp;
  }
  
  if(pmath_is_mpfloat(obj)) {
    fmpz_t mant;
    fmpz_t exp;
    pmath_integer_t mant_obj;
    pmath_integer_t exp_obj;
    
    fmpz_init(mant);
    fmpz_init(exp);
    
    arf_get_fmpz_2exp(mant, exp, arb_midref(PMATH_AS_ARB(obj)));
    mant_obj = _pmath_integer_from_fmpz(mant);
    exp_obj = _pmath_integer_from_fmpz(exp);
    
    fmpz_clear(mant);
    fmpz_clear(exp);
    
    pmath_unref(obj);
    if(pmath_same(exp_obj, PMATH_FROM_INT32(0)))
      return mant_obj;
    
    return TIMES(mant_obj, POW(INT(2), exp_obj));
  }
  
  return obj;
}

static pmath_t set_finite_precision_number(pmath_number_t obj, double prec) {
  pmath_float_t result;
  
  if(prec >= PMATH_MP_PREC_MAX) {
    pmath_unref(obj);
    return PMATH_NULL; // overflow message?
  }
  
  if(prec < 0)
    prec = 0; // error message?
    
  if(pmath_is_rational(obj))
    return _pmath_create_mp_float_from_q(obj, (slong)ceil(prec));
    
  result = _pmath_create_mp_float((slong)ceil(prec));
  if(pmath_is_null(result)) {
    pmath_unref(obj);
    return PMATH_NULL;
  }
  
  if(pmath_is_double(obj)) {
    arb_set_d(PMATH_AS_ARB(result), PMATH_AS_DOUBLE(obj));
    arb_set_round(PMATH_AS_ARB(result), PMATH_AS_ARB(result), PMATH_AS_ARB_WORKING_PREC(result));
  }
  else {
    assert(pmath_is_mpfloat(obj));
    
    if(PMATH_AS_ARB_WORKING_PREC(result) == PMATH_AS_ARB_WORKING_PREC(obj)) {
      pmath_unref(result);
      return obj;
    }
    arb_set_round(PMATH_AS_ARB(result), PMATH_AS_ARB(obj), PMATH_AS_ARB_WORKING_PREC(result));
  }
  
  pmath_unref(obj);
  return result;
}

struct set_precision_data_t {
  double       prec;
  pmath_t      prec_obj;
};

static pmath_t set_precision(
  pmath_t  obj, // will be freed
  size_t   dummy_index,
  void    *_data // A struct set_precision_data_t*
) {
  struct set_precision_data_t *data = _data;
  pmath_symbol_t sym;
  
START_SET_PRECISION:

  if(pmath_is_number(obj)) {
    double prec = data->prec;
    
    if(!isfinite(prec)) {
      double d;
      
      if(prec > 0)
        return set_infinite_precision_number(obj);
        
        if(pmath_is_double(obj))
        return obj;
        
      d = pmath_number_get_d(obj);
      
      if(isfinite(d)) {
        pmath_unref(obj);
        return PMATH_FROM_DOUBLE(d);
      }
      
      // TODO: warning about overflow during conversion to double.
      
      prec = DBL_MANT_DIG;
    }
    
    return set_finite_precision_number(obj, prec);
  }
  
  if(pmath_is_packed_array(obj)) {
    switch(pmath_packed_array_get_element_type(obj)) {
      case PMATH_PACKED_INT32:
        if(data->prec == HUGE_VAL)
          return obj;
          
        if(data->prec == -HUGE_VAL)
          return pmath_to_packed_array(obj, PMATH_PACKED_DOUBLE);
          
        break;
        
      case PMATH_PACKED_DOUBLE:
        if(data->prec == -HUGE_VAL)
          return obj;
          
        break;
    }
  }
  
  sym = _pmath_topmost_symbol(obj);
  if(!pmath_is_null(sym) && !pmath_same(sym, PMATH_SYMBOL_LIST)) {
    /* Rule out List(...) to be consistent with packed-array behaviour.
       Nobody should override SetPrecision({...}, ...) anyways.
     */
    
    struct _pmath_symbol_rules_t  *rules;
    pmath_t result;
    
    rules = _pmath_symbol_get_rules(sym, RULES_READ);
    
    if(rules) {
      pmath_bool_t found = FALSE;
      
      if(!found) {
        result = pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_SETPRECISION), 2,
                   pmath_ref(obj),
                   pmath_ref(data->prec_obj));
                   
        found = _pmath_rulecache_find(&rules->approx_rules, &result);
      }
      
      if(found) {
        pmath_unref(sym);
        pmath_unref(obj);
        //return result;
        obj = pmath_evaluate(result);
        
        if(pmath_aborting())
          return obj;
          
        goto START_SET_PRECISION;
      }
      
      pmath_unref(result);
    }
    
    if(_pmath_run_approx_code(sym, &obj, data->prec)) {
//      if(pmath_is_expr(obj))
//        obj = pmath_evaluate(obj);
//
//      if(!pmath_is_float(obj)) {
//        pmath_debug_print_object("[floating point number expected, but ", obj , " given]\n");
//      }

      pmath_unref(sym);
      return obj;
    }
    
    pmath_unref(sym);
  }
  
  if(pmath_is_expr(obj)) {
    obj = _pmath_expr_map(obj, 1, SIZE_MAX, set_precision, data);
    
    return obj;
  }
  
  return obj;
}

PMATH_API
pmath_t pmath_set_precision(pmath_t obj, double prec) {
  struct set_precision_data_t data;
  
  data.prec = prec;
  data.prec_obj = _pmath_from_precision(prec);
  
  obj = set_precision(obj, 1, &data);
  
  pmath_unref(data.prec_obj);
  obj = pmath_evaluate(obj);
  return obj;
}
