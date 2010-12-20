#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threads.h>

#include <pmath-builtins/arithmetic-private.h>


PMATH_API
double pmath_accuracy(pmath_t obj){ // will be freed
  double acc;
  
  if(pmath_instance_of(obj, PMATH_TYPE_MACHINE_FLOAT)){
    if(((struct _pmath_machine_float_t*)obj)->value == 0){
      pmath_unref(obj);
      return 1 - DBL_MIN_EXP;
    }
    
    acc = ((struct _pmath_machine_float_t*)obj)->value;
    
    acc = DBL_MANT_DIG - log2(fabs(acc));
    pmath_unref(obj);
    return acc;
  }
  
  if(pmath_instance_of(obj, PMATH_TYPE_MP_FLOAT)){
    long exp;
    double d = mpfr_get_d_2exp(&exp, ((struct _pmath_mp_float_t*)obj)->error, GMP_RNDN);
    //acc = dex_get_d_log2(((struct _pmath_mp_float_t*)obj)->error);
    pmath_unref(obj);
    //return - acc;
    return - exp - log2(fabs(d));
  }
  
  acc = HUGE_VAL;
  
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    size_t i;
    
    for(i = 0;i <= pmath_expr_length(obj);++i){
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
double pmath_precision(pmath_t obj){ // will be freed
// -HUGE_VAL = machine precision
  double prec;
  
  if(pmath_instance_of(obj, PMATH_TYPE_MACHINE_FLOAT)){
    pmath_unref(obj);
    return -HUGE_VAL;
  }
  
  if(pmath_instance_of(obj, PMATH_TYPE_MP_FLOAT)){
    long val_exp, err_exp;
    double val_d, err_d;
    
    if(mpfr_zero_p(((struct _pmath_mp_float_t*)obj)->value)){
      pmath_unref(obj);
      return 0.0;
    }
    
    val_d = mpfr_get_d_2exp(&val_exp, ((struct _pmath_mp_float_t*)obj)->value, GMP_RNDN);
    err_d = mpfr_get_d_2exp(&err_exp, ((struct _pmath_mp_float_t*)obj)->error, GMP_RNDN);
    
    pmath_unref(obj);
    return log2(fabs(val_d)) - log2(err_d) + val_exp - err_exp;
  }
  
  prec = HUGE_VAL;
  
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    size_t i;
    
    for(i = 0;i <= pmath_expr_length(obj);++i){
      double prec2 = pmath_precision(
        pmath_expr_get_item(obj, i));
      
      if(prec2 < prec)
        prec = prec2;
    }
  }
  
  pmath_unref(obj);
  return prec;
}


PMATH_API 
pmath_t pmath_set_accuracy(pmath_t obj, double acc){ // obj will be freed
  if(acc == -HUGE_VAL || acc == HUGE_VAL)
    return pmath_set_precision(obj, acc);
    
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    size_t i;
    for(i = 0;i < pmath_expr_length(obj);++i){
      obj = pmath_expr_set_item(
        obj, i, 
        pmath_set_accuracy(
          pmath_expr_get_item(obj, i), 
          acc));
    }
    
    return obj;
  }
  
  if(pmath_instance_of(obj, PMATH_TYPE_NUMBER)){
    struct _pmath_mp_float_t *result;
    
    double prec = 0.0;
    
    switch(obj->type_shift){
      case PMATH_TYPE_SHIFT_INTEGER: { 
        if(mpz_sgn(((struct _pmath_integer_t*)obj)->value) == 0){
          prec = 0;
        }
        else{
          long exp;
          double d;
          
          d = mpz_get_d_2exp(&exp, ((struct _pmath_integer_t*)obj)->value);
          
          prec = log2(fabs(d)) + exp + acc;
        }
      } break;
      
      case PMATH_TYPE_SHIFT_QUOTIENT: {
        long numexp, denexp;
        double numd, dend;
        
        numd = mpz_get_d_2exp(&numexp, ((struct _pmath_quotient_t*)obj)->numerator->value);
        dend = mpz_get_d_2exp(&denexp, ((struct _pmath_quotient_t*)obj)->denominator->value);
        
        prec = log2(fabs(numd)) - log2(fabs(dend)) + numexp - denexp + acc;
      } break;
      
      case PMATH_TYPE_SHIFT_MACHINE_FLOAT: {
        if(((struct _pmath_machine_float_t*)obj)->value == 0){
          pmath_unref(obj);
          return pmath_integer_new_si(0);
        }
          
        prec = log2(fabs(((struct _pmath_machine_float_t*)obj)->value)) + acc;
      } break;
      
      case PMATH_TYPE_SHIFT_MP_FLOAT: {
        long exp;
        double d = mpfr_get_d_2exp(&exp, ((struct _pmath_mp_float_t*)obj)->value, GMP_RNDN);
        
        prec = log2(fabs(d)) + exp + acc;
      } break;
    }
  
    if(prec >= PMATH_MP_PREC_MAX){
      pmath_unref(obj);
      return NULL; // overflow message?
    }
    
    result = _pmath_create_mp_float(prec >= MPFR_PREC_MIN ? (mp_prec_t)ceil(prec) : MPFR_PREC_MIN);
    if(!result){
      pmath_unref(obj);
      return NULL;
    }
    
    switch(obj->type_shift){
      case PMATH_TYPE_SHIFT_INTEGER: {
        mpfr_set_z(
          result->value, 
          ((struct _pmath_integer_t*)obj)->value,
          GMP_RNDN);
      } break;
      
      case PMATH_TYPE_SHIFT_QUOTIENT: {
        mpfr_set_z(
          result->value, 
          ((struct _pmath_quotient_t*)obj)->numerator->value,
          GMP_RNDN);
          
        mpfr_div_z(
          result->value, 
          result->value,
          ((struct _pmath_quotient_t*)obj)->denominator->value,
          GMP_RNDN);
      } break;
      
      case PMATH_TYPE_SHIFT_MACHINE_FLOAT: {
        mpfr_set_d(
          result->value,
          ((struct _pmath_machine_float_t*)obj)->value,
          GMP_RNDN);
      } break;
      
      case PMATH_TYPE_SHIFT_MP_FLOAT: {
        mpfr_set(
          result->value,
          ((struct _pmath_mp_float_t*)obj)->value,
          GMP_RNDN);
      } break;
    }
    
    mpfr_set_d(result->error, -acc, GMP_RNDN);
    mpfr_ui_pow(result->error, 2, result->error, GMP_RNDN);
    
    pmath_unref(obj);
    return (pmath_float_t)result;
  }
  
  return obj;
}

PMATH_API 
pmath_t pmath_set_precision(pmath_t obj, double prec){
  if(pmath_instance_of(obj, PMATH_TYPE_EXPRESSION)){
    size_t i;
    for(i = 0;i < pmath_expr_length(obj);++i){
      obj = pmath_expr_set_item(
        obj, i, 
        pmath_set_precision(
          pmath_expr_get_item(obj, i), 
          prec));
    }
    
    return obj;
  }
  
  if(pmath_instance_of(obj, PMATH_TYPE_NUMBER)){
    struct _pmath_mp_float_t *result;
    
    if(!isfinite(prec)){
      double d;
      
      if(prec > 0){
        if(obj->type_shift == PMATH_TYPE_SHIFT_MACHINE_FLOAT){
          struct _pmath_machine_float_t *mf = (struct _pmath_machine_float_t*)obj;
          struct _pmath_mp_float_t *mp = _pmath_create_mp_float_from_d(mf->value);
          
          if(mp){
            pmath_unref(obj);
            obj = (pmath_t)mp;
          }
          else
            return obj;
        }
        
        if(obj->type_shift == PMATH_TYPE_SHIFT_MP_FLOAT){
          struct _pmath_integer_t *num;
          struct _pmath_mp_float_t *mf = (struct _pmath_mp_float_t*)obj;
          mpfr_exp_t exp;
          
          num = _pmath_create_integer();
          if(num){
            exp = mpfr_get_z_2exp(num->value, mf->value);
            
            if(exp < 0 && mpz_sgn(num->value) != 0){
              struct _pmath_integer_t *den = _pmath_create_integer();
              
              if(den){
                mpz_set_ui(den->value, 1);
                mpz_mul_2exp(den->value, den->value, (unsigned long int)-exp);
                
                pmath_unref(obj);
                return pmath_rational_new((pmath_integer_t)num, (pmath_integer_t)den);
              }
            }
            else{
              mpz_mul_2exp(num->value, num->value, (unsigned long int)exp);
              pmath_unref(obj);
              return (pmath_integer_t)num;
            }

            pmath_unref((pmath_integer_t)num);
          }
          
          return obj;
        }
        
        return obj;
      }
      
      if(obj->type_shift == PMATH_TYPE_SHIFT_MACHINE_FLOAT)
        return obj;
        
      d = pmath_number_get_d(obj);
      
      if(isfinite(d)){
        pmath_unref(obj);
        return pmath_float_new_d(d);
      }
      
      prec = DBL_MANT_DIG;  
    }
    
    if(pmath_number_sign(obj) == 0){
      pmath_unref(obj);
      return pmath_integer_new_si(0);
    }
    
    if(prec >= PMATH_MP_PREC_MAX){
      pmath_unref(obj);
      return NULL; // overflow message?
    }
    
    result = _pmath_create_mp_float(prec >= MPFR_PREC_MIN ? (mp_prec_t)ceil(prec) : MPFR_PREC_MIN);
    if(!result){
      pmath_unref(obj);
      return NULL;
    }
    
    switch(obj->type_shift){
      case PMATH_TYPE_SHIFT_INTEGER: {
        mpfr_set_z(
          result->value, 
          ((struct _pmath_integer_t*)obj)->value,
          GMP_RNDN);
      } break;
      
      case PMATH_TYPE_SHIFT_QUOTIENT: {
        mpfr_set_z(
          result->value, 
          ((struct _pmath_quotient_t*)obj)->numerator->value,
          GMP_RNDN);
          
        mpfr_div_z(
          result->value, 
          result->value,
          ((struct _pmath_quotient_t*)obj)->denominator->value,
          GMP_RNDN);
      } break;
      
      case PMATH_TYPE_SHIFT_MACHINE_FLOAT: {
        mpfr_set_d(
          result->value,
          ((struct _pmath_machine_float_t*)obj)->value,
          GMP_RNDN);
      } break;
      
      case PMATH_TYPE_SHIFT_MP_FLOAT: {
        mpfr_set(
          result->value,
          ((struct _pmath_mp_float_t*)obj)->value,
          GMP_RNDN);
      } break;
    }
  
    // error = |value| * 2 ^ -bits
    mpfr_set_d(result->error, -prec, GMP_RNDU);
    
    mpfr_ui_pow(
      result->error,
      2,
      result->error,
      GMP_RNDU);
    
    mpfr_mul(
      result->error,
      result->error,
      result->value,
      mpfr_sgn(result->value) < 0 ? GMP_RNDD : GMP_RNDU);
    
    mpfr_abs(result->error, result->error, GMP_RNDU);
    
    pmath_unref(obj);
    return (pmath_float_t)result;
  }
  
  return obj;
}


PMATH_API pmath_t pmath_approximate(
  pmath_t obj, // will be freed
  double precision_goal,
  double accuracy_goal
){
  double prec, acc, prec2, acc2;
  
  prec = precision_goal;
  acc  = accuracy_goal;
  
  while(!pmath_aborting()){
    pmath_t res = _pmath_approximate_step(
      pmath_ref(obj), prec, acc);
      
    prec2 = pmath_precision(pmath_ref(res));
    acc2  = pmath_accuracy( pmath_ref(res));
    
    if(prec2 >= precision_goal || acc2 >= accuracy_goal){
      pmath_unref(obj);
      return res;
    }
    
    if(prec2 < prec)
      prec = precision_goal + prec - prec2 + 2;
    if(acc2 < acc)
      acc = accuracy_goal + acc - acc2 + 2;
    
    if(prec > precision_goal + pmath_max_extra_precision
    || acc  > accuracy_goal  + pmath_max_extra_precision){
      // max. extra precision reached
      pmath_unref(obj);
      return res;
    }
    
    pmath_unref(res);
  }
  
  return NULL;
}
