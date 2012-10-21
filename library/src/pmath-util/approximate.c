#include <pmath-util/approximate.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>

/* A value x with uncertainity u is realy any value in [x-u/2, x+u/2]
   An approximate number with accuracy acc_b (in base b) by definition has
   uncertainity u = b^-acc_b. So acc_b = -Log(b, u).
   A non-zero approximate number with precision prec_b (in base b) by definition
   has uncertainity u = |x| b^-prec_b.
   So prec_b = -Log(b, u / |x|) = Log(b, |x|) + acc_b

   Note that PMATH_AS_MP_ERROR(x) =: dx = u/2.
   So acc_2 = -Log(2, u) = -Log(2, 2 dx) = -(1 + Log(2, dx)).
   And dx = 1/2 * 2^-acc_2 = 2^-(acc_2 + 1)
          = 1/2 * |x| * 2^-prec_2 = |x| * 2^-(prec_2 + 1)

   Internally, we use base b = 2.
 */

PMATH_API
double pmath_accuracy(pmath_t obj) { // will be freed
  double acc;
  
  if(pmath_is_double(obj)) {
    if(PMATH_AS_DOUBLE(obj) == 0) {
      pmath_unref(obj);
      return 1 - DBL_MIN_EXP;
    }
    
    acc = PMATH_AS_DOUBLE(obj);
    
    acc = DBL_MANT_DIG - log2(fabs(acc));
    pmath_unref(obj);
    return acc;
  }
  
  if(pmath_is_mpfloat(obj)) {
    long exp;
    double d = mpfr_get_d_2exp(&exp, PMATH_AS_MP_ERROR(obj), MPFR_RNDN);
    //acc = dex_get_d_log2(PMATH_AS_MP_ERROR(obj));
    pmath_unref(obj);
    //return - acc;
    return -(log2(d) + exp + 1);
  }
  
  acc = HUGE_VAL;
  
  if(pmath_is_expr(obj)) {
    size_t i;
    
    for(i = 0; i <= pmath_expr_length(obj); ++i) {
      double acc2 = pmath_accuracy(
                      pmath_expr_get_item(obj, i));
                      
      if(acc2 < acc)
        acc = acc2;
    }
  }
  
  pmath_unref(obj);
  return acc;
}

PMATH_API
double pmath_precision(pmath_t obj) { // will be freed
// -HUGE_VAL = machine precision
  double prec;
  
  if(pmath_is_double(obj)) {
    pmath_unref(obj);
    return -HUGE_VAL;
  }
  
  if(pmath_is_mpfloat(obj)) {
    long val_exp, err_exp;
    double val_d, err_d;
    
    if(mpfr_zero_p(PMATH_AS_MP_VALUE(obj))) {
      pmath_unref(obj);
      return 0.0;
    }
    
    val_d = mpfr_get_d_2exp(&val_exp, PMATH_AS_MP_VALUE(obj), MPFR_RNDN);
    err_d = mpfr_get_d_2exp(&err_exp, PMATH_AS_MP_ERROR(obj), MPFR_RNDN);
    
    pmath_unref(obj);
    return log2(fabs(val_d)) + val_exp - (log2(err_d) + err_exp + 1);
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


PMATH_API
pmath_t pmath_set_accuracy(pmath_t obj, double acc) { // obj will be freed
  if(acc == -HUGE_VAL || acc == HUGE_VAL)
    return pmath_set_precision(obj, acc);
    
  if(pmath_is_expr(obj)) {
    size_t i;
    for(i = 0; i < pmath_expr_length(obj); ++i) {
      obj = pmath_expr_set_item(
              obj, i,
              pmath_set_accuracy(
                pmath_expr_get_item(obj, i),
                acc));
    }
    
    return obj;
  }
  
  if(pmath_is_double(obj)) {
    pmath_float_t result;
    
    if(PMATH_AS_DOUBLE(obj) == 0) {
      if(acc >= PMATH_MP_PREC_MAX)
        return pmath_ref(_pmath_object_underflow); // underflow message?
        
      result = _pmath_create_mp_float(acc >= MPFR_PREC_MIN ? (mpfr_prec_t)ceil(acc) : MPFR_PREC_MIN);
      
      if(pmath_is_null(result))
        return PMATH_NULL;
        
      mpfr_set_ui(
        PMATH_AS_MP_VALUE(result),
        0,
        MPFR_RNDN);
    }
    else {
      double prec = log2(fabs(PMATH_AS_DOUBLE(obj))) + acc;
      if(prec >= PMATH_MP_PREC_MAX)
        return pmath_ref(_pmath_object_overflow); // overflow message?
        
      result = _pmath_create_mp_float(prec >= MPFR_PREC_MIN ? (mpfr_prec_t)ceil(prec) : MPFR_PREC_MIN);
      if(pmath_is_null(result))
        return PMATH_NULL;
        
      mpfr_set_d(
        PMATH_AS_MP_VALUE(result),
        PMATH_AS_DOUBLE(obj),
        MPFR_RNDN);
    }
    
    mpfr_set_d( PMATH_AS_MP_ERROR(result), -acc - 1, MPFR_RNDN);
    mpfr_ui_pow(PMATH_AS_MP_ERROR(result), 2, PMATH_AS_MP_ERROR(result), MPFR_RNDN);
    
    return result;
  }
  
  if(pmath_is_number(obj)) {
    pmath_float_t result;
    double prec = 0.0;
    
    if(pmath_is_int32(obj)) {
      if(PMATH_AS_INT32(obj) == 0) {
        prec = acc;
      }
      else {
        int exp;
        double d;
        
        d = frexp((double)PMATH_AS_INT32(obj), &exp);
        
        prec = log2(fabs(d)) + exp + acc;
      }
    }
    else {
      assert(pmath_is_pointer(obj));
      assert(PMATH_AS_PTR(obj) != NULL);
      
      switch(PMATH_AS_PTR(obj)->type_shift) {
        case PMATH_TYPE_SHIFT_MP_INT:
          if(mpz_sgn(PMATH_AS_MPZ(obj)) == 0) {
            prec = acc;
          }
          else {
            long exp;
            double d;
            
            d = mpz_get_d_2exp(&exp, PMATH_AS_MPZ(obj));
            
            prec = log2(fabs(d)) + exp + acc;
          }
          break;
          
        case PMATH_TYPE_SHIFT_QUOTIENT: {
            long numexp, denexp;
            double numd, dend;
            
            if(pmath_is_int32(PMATH_QUOT_NUM(obj))) {
              int i;
              numd = frexp((double)PMATH_AS_INT32(PMATH_QUOT_NUM(obj)), &i);
              numexp = i;
            }
            else
              numd = mpz_get_d_2exp(&numexp, PMATH_AS_MPZ(PMATH_QUOT_NUM(obj)));
              
            if(pmath_is_int32(PMATH_QUOT_DEN(obj))) {
              int i;
              dend = frexp((double)PMATH_AS_INT32(PMATH_QUOT_DEN(obj)), &i);
              denexp = i;
            }
            else
              dend = mpz_get_d_2exp(&denexp, PMATH_AS_MPZ(PMATH_QUOT_DEN(obj)));
              
            prec = log2(fabs(numd)) - log2(fabs(dend)) + numexp - denexp + acc;
          } break;
          
        case PMATH_TYPE_SHIFT_MP_FLOAT:
          if(mpfr_zero_p(PMATH_AS_MP_VALUE(obj))) {
            prec = acc;
          }
          else {
            long exp;
            double d = mpfr_get_d_2exp(&exp, PMATH_AS_MP_VALUE(obj), MPFR_RNDN);
            
            prec = log2(fabs(d)) + exp + acc;
          }
          break;
          
        default:
          assert("unknown number type" && 0);
      }
    }
    
    if(prec >= PMATH_MP_PREC_MAX) {
      pmath_unref(obj);
      return pmath_ref(_pmath_object_overflow); // overflow message?
    }
    
    result = _pmath_create_mp_float(prec >= MPFR_PREC_MIN ? (mpfr_prec_t)ceil(prec) : MPFR_PREC_MIN);
    if(pmath_is_null(result)) {
      pmath_unref(obj);
      return PMATH_NULL;
    }
    
    if(pmath_is_int32(obj)) {
      mpfr_set_si(
        PMATH_AS_MP_VALUE(result),
        PMATH_AS_INT32(obj),
        MPFR_RNDN);
    }
    else switch(PMATH_AS_PTR(obj)->type_shift) {
        case PMATH_TYPE_SHIFT_MP_INT: {
            mpfr_set_z(
              PMATH_AS_MP_VALUE(result),
              PMATH_AS_MPZ(obj),
              MPFR_RNDN);
          } break;
          
        case PMATH_TYPE_SHIFT_QUOTIENT: {
            if(pmath_is_int32(PMATH_QUOT_NUM(obj))) {
              mpfr_set_si(
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_INT32(PMATH_QUOT_NUM(obj)),
                MPFR_RNDN);
            }
            else {
              assert(pmath_is_mpint(PMATH_QUOT_NUM(obj)));
              
              mpfr_set_z(
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_MPZ(PMATH_QUOT_NUM(obj)),
                MPFR_RNDN);
            }
            
            if(pmath_is_int32(PMATH_QUOT_DEN(obj))) {
              mpfr_div_si(
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_INT32(PMATH_QUOT_DEN(obj)),
                MPFR_RNDN);
            }
            else {
              assert(pmath_is_mpint(PMATH_QUOT_DEN(obj)));
              
              mpfr_div_z(
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_MPZ(PMATH_QUOT_DEN(obj)),
                MPFR_RNDN);
            }
          } break;
          
        case PMATH_TYPE_SHIFT_MP_FLOAT: {
            mpfr_set(
              PMATH_AS_MP_VALUE(result),
              PMATH_AS_MP_VALUE(obj),
              MPFR_RNDN);
              
            if(mpfr_zero_p(PMATH_AS_MP_VALUE(obj))) {
              mpfr_set_ui(
                PMATH_AS_MP_VALUE(result),
                +0,
                MPFR_RNDN);
            }
            else {
              mpfr_set(
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_MP_VALUE(obj),
                MPFR_RNDN);
            }
          } break;
      }
      
    mpfr_set_d( PMATH_AS_MP_ERROR(result), -acc - 1, MPFR_RNDN);
    mpfr_ui_pow(PMATH_AS_MP_ERROR(result), 2, PMATH_AS_MP_ERROR(result), MPFR_RNDN);
    
    pmath_unref(obj);
    return result;
  }
  
  return obj;
}

PMATH_API
pmath_t pmath_set_precision(pmath_t obj, double prec) {
  if(pmath_is_expr(obj)) {
    size_t i;
    for(i = 0; i < pmath_expr_length(obj); ++i) {
      obj = pmath_expr_set_item(
              obj, i,
              pmath_set_precision(
                pmath_expr_get_item(obj, i),
                prec));
    }
    
    return obj;
  }
  
  if(pmath_is_number(obj)) {
    pmath_float_t result;
    
    if(!isfinite(prec)) {
      double d;
      
      if(prec > 0) {
        if(pmath_is_double(obj)) {
          pmath_float_t mp = _pmath_create_mp_float_from_d(PMATH_AS_DOUBLE(obj));
          
          if(pmath_is_null(mp))
            return obj;
            
          pmath_unref(obj);
          obj = mp;
        }
        
        if(pmath_is_mpfloat(obj)) {
          pmath_mpint_t num;
          mpfr_exp_t exp;
          
          num = _pmath_create_mp_int(0);
          if(!pmath_is_null(num)) {
            exp = mpfr_get_z_2exp(PMATH_AS_MPZ(num), PMATH_AS_MP_VALUE(obj));
            
            if(exp < 0 && mpz_sgn(PMATH_AS_MPZ(num)) != 0) {
              pmath_mpint_t den = _pmath_create_mp_int(0);
              
              if(!pmath_is_null(den)) {
                mpz_set_ui(PMATH_AS_MPZ(den), 1);
                mpz_mul_2exp(PMATH_AS_MPZ(den), PMATH_AS_MPZ(den), (unsigned long int) - exp);
                
                pmath_unref(obj);
                return pmath_rational_new(_pmath_mp_int_normalize(num), _pmath_mp_int_normalize(den));
              }
            }
            else {
              mpz_mul_2exp(PMATH_AS_MPZ(num), PMATH_AS_MPZ(num), (unsigned long int)exp);
              pmath_unref(obj);
              return _pmath_mp_int_normalize(num);
            }
            
            pmath_unref(num);
          }
          
          return obj;
        }
        
        return obj;
      }
      
      if(pmath_is_double(obj))
        return obj;
        
      d = pmath_number_get_d(obj);
      
      if(isfinite(d)) {
        pmath_unref(obj);
        return PMATH_FROM_DOUBLE(d);
      }
      
      prec = DBL_MANT_DIG;
    }
    
    if(pmath_number_sign(obj) == 0) {
      pmath_unref(obj);
      return PMATH_FROM_INT32(0);
    }
    
    if(prec >= PMATH_MP_PREC_MAX) {
      pmath_unref(obj);
      return PMATH_NULL; // overflow message?
    }
    
    result = _pmath_create_mp_float(prec >= MPFR_PREC_MIN ? (mpfr_prec_t)ceil(prec) : MPFR_PREC_MIN);
    if(pmath_is_null(result)) {
      pmath_unref(obj);
      return PMATH_NULL;
    }
    
    if(pmath_is_double(obj)) {
      mpfr_set_d(
        PMATH_AS_MP_VALUE(result),
        PMATH_AS_DOUBLE(obj),
        MPFR_RNDN);
    }
    else if(pmath_is_int32(obj)) {
      mpfr_set_si(
        PMATH_AS_MP_VALUE(result),
        PMATH_AS_INT32(obj),
        MPFR_RNDN);
    }
    else switch(PMATH_AS_PTR(obj)->type_shift) {
        case PMATH_TYPE_SHIFT_MP_INT: {
            mpfr_set_z(
              PMATH_AS_MP_VALUE(result),
              PMATH_AS_MPZ(obj),
              MPFR_RNDN);
          } break;
          
        case PMATH_TYPE_SHIFT_QUOTIENT: {
            if(pmath_is_int32(PMATH_QUOT_NUM(obj))) {
              mpfr_set_si(
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_INT32(PMATH_QUOT_NUM(obj)),
                MPFR_RNDN);
            }
            else {
              assert(pmath_is_mpint(PMATH_QUOT_NUM(obj)));
              
              mpfr_set_z(
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_MPZ(PMATH_QUOT_NUM(obj)),
                MPFR_RNDN);
            }
            
            if(pmath_is_int32(PMATH_QUOT_DEN(obj))) {
              mpfr_div_si(
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_INT32(PMATH_QUOT_DEN(obj)),
                MPFR_RNDN);
            }
            else {
              assert(pmath_is_mpint(PMATH_QUOT_DEN(obj)));
              
              mpfr_div_z(
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_MP_VALUE(result),
                PMATH_AS_MPZ(PMATH_QUOT_DEN(obj)),
                MPFR_RNDN);
            }
          } break;
          
        case PMATH_TYPE_SHIFT_MP_FLOAT: {
            mpfr_set(
              PMATH_AS_MP_VALUE(result),
              PMATH_AS_MP_VALUE(obj),
              MPFR_RNDN);
          } break;
      }
      
    mpfr_set_d(PMATH_AS_MP_ERROR(result), -prec - 1, MPFR_RNDU);
    
    mpfr_ui_pow(
      PMATH_AS_MP_ERROR(result),
      2,
      PMATH_AS_MP_ERROR(result),
      MPFR_RNDU);
      
    mpfr_mul(
      PMATH_AS_MP_ERROR(result),
      PMATH_AS_MP_ERROR(result),
      PMATH_AS_MP_VALUE(result),
      MPFR_RNDA);
      
    mpfr_abs(PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(result), MPFR_RNDU);
    
    pmath_unref(obj);
    return result;
  }
  
  return obj;
}


PMATH_API pmath_t pmath_approximate(
  pmath_t       obj,            // will be freed
  double        precision_goal, // -inf = MachinePrecision
  double        accuracy_goal,  // -inf = MachinePrecision
  pmath_bool_t *aborted
) {
  double prec, acc, prec2, acc2;
  pmath_thread_t me = pmath_thread_get_current();
  
  if(!me)
    return obj;
    
  if(aborted)
    *aborted = FALSE;
    
  prec = precision_goal;
  acc  = accuracy_goal;
  
  while(!pmath_aborting()) {
    pmath_t res = pmath_evaluate(_pmath_approximate_step(
                                   pmath_ref(obj), prec, acc));
                                   
    prec2 = pmath_precision(pmath_ref(res));
    acc2  = pmath_accuracy( pmath_ref(res));
    
    if( prec2 >= precision_goal ||
        acc2  >= accuracy_goal ||
        pmath_equals(res, obj))
    {
      pmath_unref(obj);
      return res;
    }
    
    if(prec2 < prec)
      prec = 2 * prec - prec2 + 2;
    if(acc2 < acc)
      acc = 2 * acc - acc2 + 2;
      
    pmath_unref(res);
    if( prec > precision_goal + me->max_extra_precision ||
        acc  > accuracy_goal  + me->max_extra_precision)
    {
      // max. extra precision reached
      if(aborted) {
        *aborted = TRUE;
      }
      else {
        pmath_message(PMATH_SYMBOL_N, "meprec", 2,
                      pmath_evaluate(pmath_ref(PMATH_SYMBOL_MAXEXTRAPRECISION)),
                      pmath_ref(obj));
      }
      
      prec = precision_goal + me->max_extra_precision;
      acc  = accuracy_goal  + me->max_extra_precision;
      
      return pmath_evaluate(_pmath_approximate_step(obj, prec, acc));
    }
  }
  
  return obj;
}
