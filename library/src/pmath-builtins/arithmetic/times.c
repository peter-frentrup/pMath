#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/number-theory-private.h>

static pmath_rational_t _mul_qi(
  pmath_quotient_t quotA, // will be freed. not NULL!
  pmath_integer_t  intB   // will be freed. not NULL!
){ // u/v * w = (u*w)/v
  pmath_integer_t numerator =
    (pmath_integer_t)pmath_ref(
      (pmath_integer_t)((struct _pmath_quotient_t*)quotA)->numerator);
  pmath_integer_t denominator =
    (pmath_integer_t)pmath_ref(
      (pmath_integer_t)((struct _pmath_quotient_t*)quotA)->denominator);
  pmath_unref(quotA);

  return pmath_rational_new(
    _mul_ii(numerator, intB),
    denominator);
}

static pmath_rational_t _mul_qq(
  pmath_quotient_t quotA, // will be freed. not NULL!
  pmath_quotient_t quotB  // will be freed. not NULL!
){ // u/v * w/x = (u*w)/(v*x)
  pmath_integer_t numeratorA =
    (pmath_integer_t)pmath_ref(
      (pmath_integer_t)((struct _pmath_quotient_t*)quotA)->numerator);
  pmath_integer_t denominatorA =
    (pmath_integer_t)pmath_ref(
      (pmath_integer_t)((struct _pmath_quotient_t*)quotA)->denominator);

  pmath_integer_t numeratorB =
    (pmath_integer_t)pmath_ref(
      (pmath_integer_t)((struct _pmath_quotient_t*)quotB)->numerator);
  pmath_integer_t denominatorB =
    (pmath_integer_t)pmath_ref(
      (pmath_integer_t)((struct _pmath_quotient_t*)quotB)->denominator);

  pmath_unref(quotA);
  pmath_unref(quotB);

  return pmath_rational_new(
    _mul_ii(numeratorA, numeratorB),
    _mul_ii(denominatorA, denominatorB));
}

static pmath_float_t _mul_fi(
  pmath_float_t   floatA, // will be freed. not NULL!
  pmath_integer_t intB   // will be freed. not NULL!
){
  struct _pmath_mp_float_t *result;
  double fprec;
  long   expB;
  mp_prec_t prec;
  int signB;
  
  signB = mpz_sgn(((struct _pmath_integer_t*)intB)->value);
  if(signB == 0){
    pmath_unref(floatA);
    return intB;
  }
  
  fprec = pmath_precision(pmath_ref(floatA));
  fprec+= log2(fabs(mpz_get_d_2exp(&expB, ((struct _pmath_integer_t*)intB)->value)));
  fprec+= expB;
  
  if(fprec < MPFR_PREC_MIN)
    prec = MPFR_PREC_MIN;
  else if(fprec > PMATH_MP_PREC_MAX)
    prec = PMATH_MP_PREC_MAX;
  else
    prec = (mp_prec_t)ceil(fprec);
  
  result = _pmath_create_mp_float(prec);
  
  if(!result){
    pmath_unref(floatA);
    pmath_unref(intB);
    return NULL;
  }
  
  // dxy = y dx + x dy = y dx   (dy = 0)
  mpfr_mul_z(
    result->error, 
    ((struct _pmath_mp_float_t*)floatA)->error,
    ((struct _pmath_integer_t*)intB)->value,
    signB > 0 ? GMP_RNDU : GMP_RNDD);
  
  mpfr_abs(result->error, result->error, GMP_RNDN);
  
  mpfr_mul_z(
    result->value,
    ((struct _pmath_mp_float_t*)floatA)->value,
    ((struct _pmath_integer_t*)intB)->value,
    GMP_RNDN);
  
  //_pmath_mp_float_normalize(result);

  pmath_unref(floatA);
  pmath_unref(intB);
  return (pmath_float_t)result;
}

  static pmath_float_t _div_fi(
    pmath_float_t            floatA, // will be freed. not NULL!
    struct _pmath_integer_t *intB    // wont be freed. not NULL! not zero!
  ){
    struct _pmath_mp_float_t *result;
    int signB;
    
    signB = mpz_sgn(intB->value);
    assert(signB != 0);
    
    result = _pmath_create_mp_float(
      mpfr_get_prec(((struct _pmath_mp_float_t*)floatA)->value));
    
    if(!result){
      pmath_unref(floatA);
      return NULL;
    }
    
    // d(x/y) = 1/y dx + x d(1/y) = 1/y dx   (d(1/y) = 0)
    mpfr_div_z(
      result->error, 
      ((struct _pmath_mp_float_t*)floatA)->error,
      intB->value,
      signB > 0 ? GMP_RNDU : GMP_RNDD);
    
    mpfr_abs(result->error, result->error, GMP_RNDN);
    
    mpfr_div_z(
      result->value,
      ((struct _pmath_mp_float_t*)floatA)->value,
      intB->value,
      GMP_RNDN);
    
    _pmath_mp_float_normalize(result);

    pmath_unref(floatA);
    return (pmath_float_t)result;
  }

static pmath_float_t _mul_fq(
  pmath_float_t    floatA, // will be freed. not NULL!
  pmath_quotient_t quotB   // will be freed. not NULL!
){
  floatA = _mul_fi(
    floatA, 
    pmath_ref((pmath_t)((struct _pmath_quotient_t*)quotB)->numerator));
  
  if(pmath_instance_of(floatA, PMATH_TYPE_MP_FLOAT)){
    floatA = _div_fi(floatA, ((struct _pmath_quotient_t*)quotB)->denominator);
    pmath_unref(quotB);
  }
  
  return floatA;
}

static pmath_float_t _mul_ff(
  pmath_float_t   floatA, // will be freed. not NULL!
  pmath_integer_t floatB  // will be freed. not NULL!
){
  struct _pmath_mp_float_t *result;
  struct _pmath_mp_float_t *tmp_err;
  double fprec;
  mp_prec_t prec;
  
  fprec = pmath_precision(pmath_ref(floatA)) 
        + pmath_precision(pmath_ref(floatB));
  
  if(fprec < MPFR_PREC_MIN)
    prec = MPFR_PREC_MIN;
  else if(fprec > PMATH_MP_PREC_MAX)
    prec = PMATH_MP_PREC_MAX;
  else
    prec = (mp_prec_t)ceil(fprec);
  
  result = _pmath_create_mp_float(prec);
  tmp_err = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
  
  if(!result || !tmp_err){
    pmath_unref((pmath_t)result);
    pmath_unref((pmath_t)tmp_err);
    pmath_unref(floatA);
    pmath_unref(floatB);
    return NULL;
  }
  
  // dxy = y dx + x dy
  
  mpfr_mul(
    tmp_err->value,
    ((struct _pmath_mp_float_t*)floatB)->value,
    ((struct _pmath_mp_float_t*)floatA)->error,
    mpfr_sgn(((struct _pmath_mp_float_t*)floatB)->value) > 0 ? GMP_RNDU : GMP_RNDD);
  
  mpfr_abs(tmp_err->value, tmp_err->value, GMP_RNDN);
  
  mpfr_mul(
    tmp_err->error,
    ((struct _pmath_mp_float_t*)floatA)->value,
    ((struct _pmath_mp_float_t*)floatB)->error,
    mpfr_sgn(((struct _pmath_mp_float_t*)floatA)->value) > 0 ? GMP_RNDU : GMP_RNDD);
  
  mpfr_abs(tmp_err->error, tmp_err->error, GMP_RNDN);
  
  mpfr_add(
    result->error,
    tmp_err->value,
    tmp_err->error,
    GMP_RNDU);
  
  mpfr_mul(
    result->value,
    ((struct _pmath_mp_float_t*)floatA)->value,
    ((struct _pmath_mp_float_t*)floatB)->value,
    GMP_RNDN);
  
  _pmath_mp_float_normalize(result);
  
  pmath_unref(floatA);
  pmath_unref(floatB);
  pmath_unref((pmath_t)tmp_err);
  return (pmath_float_t)result;
}

static pmath_t _mul_mi(
  pmath_float_t   floatA, // will be freed. not NULL!
  pmath_integer_t intB    // will be freed. not NULL!
){
  double d = ((struct _pmath_machine_float_t*)floatA)->value
    * mpz_get_d(((struct _pmath_integer_t*)intB)->value);
  
  if(!isfinite(d))
    return _mul_nn((pmath_float_t)_pmath_convert_to_mp_float(floatA), intB);
  
  pmath_unref(intB);
  
  if(floatA->refcount > 1){
    pmath_unref(floatA);
    return pmath_float_new_d(d);
  }
  
  ((struct _pmath_machine_float_t*)floatA)->value = d;
  return (pmath_float_t)floatA;
}

static pmath_t _mul_mq(
  pmath_float_t    floatA, // will be freed. not NULL!
  pmath_quotient_t quotB   // will be freed. not NULL!
){
  double d;
  
  d = mpz_get_d(((struct _pmath_quotient_t*)quotB)->numerator->value);
  d/= mpz_get_d(((struct _pmath_quotient_t*)quotB)->denominator->value);
  
  d*= ((struct _pmath_machine_float_t*)floatA)->value;
  
  if(!isfinite(d))
    return _mul_nn((pmath_float_t)_pmath_convert_to_mp_float(floatA), quotB);
  
  pmath_unref(quotB);
  
  if(floatA->refcount > 1){
    pmath_unref(floatA);
    return pmath_float_new_d(d);
  }
  
  ((struct _pmath_machine_float_t*)floatA)->value = d;
  return (pmath_float_t)floatA;
}

static pmath_t _mul_mf(
  pmath_float_t  floatA, // will be freed. not NULL!
  pmath_float_t  floatB  // will be freed. not NULL!
){
  double d = ((struct _pmath_machine_float_t*)floatA)->value
    * mpfr_get_d(((struct _pmath_mp_float_t*)floatB)->value, GMP_RNDN);
  
  if(!isfinite(d))
    return _mul_nn((pmath_float_t)_pmath_convert_to_mp_float(floatA), floatB);
  
  pmath_unref(floatB);
  
  if(floatA->refcount > 1){
    pmath_unref(floatA);
    return pmath_float_new_d(d);
  }
  
  ((struct _pmath_machine_float_t*)floatA)->value = d;
  return (pmath_float_t)floatA;
}

static pmath_t _mul_mm(
  pmath_float_t  floatA, // will be freed. not NULL!
  pmath_float_t  floatB  // will be freed. not NULL!
){
  double d = ((struct _pmath_machine_float_t*)floatA)->value
           * ((struct _pmath_machine_float_t*)floatB)->value;
          
  if(!isfinite(d))
    return _mul_nn(
      (pmath_float_t)_pmath_convert_to_mp_float(floatA), 
      (pmath_float_t)_pmath_convert_to_mp_float(floatB));
  
  if(floatA->refcount == 1){
    ((struct _pmath_machine_float_t*)floatA)->value = d;
    pmath_unref(floatB);
    return (pmath_float_t)floatA;
  }
  
  if(floatB->refcount == 1){
    ((struct _pmath_machine_float_t*)floatB)->value = d;
    pmath_unref(floatA);
    return (pmath_float_t)floatB;
  }
  
  pmath_unref(floatA);
  pmath_unref(floatB);
  
  return pmath_float_new_d(d);
}

PMATH_PRIVATE pmath_number_t _mul_nn(
  pmath_number_t numA, // will be freed.
  pmath_number_t numB  // will be freed.
){
  if(!numA || !numB){
    pmath_unref(numA);
    pmath_unref(numB);
    return NULL;
  }
  
  switch(numA->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER: {
      switch(numB->type_shift){
        case PMATH_TYPE_SHIFT_INTEGER:        return _mul_ii(numA, numB);
        case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_qi(numB, numA);
        case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_fi(numB, numA);
        case PMATH_TYPE_SHIFT_MACHINE_FLOAT:  return _mul_mi(numB, numA);
      }
    } break;
    
    case PMATH_TYPE_SHIFT_QUOTIENT: {
      switch(numB->type_shift){
        case PMATH_TYPE_SHIFT_INTEGER:        return _mul_qi(numA, numB);
        case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_qq(numA, numB);
        case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_fq(numB, numA);
        case PMATH_TYPE_SHIFT_MACHINE_FLOAT:  return _mul_mq(numB, numA);
      }
    } break;
    
    case PMATH_TYPE_SHIFT_MP_FLOAT: {
      switch(numB->type_shift){
        case PMATH_TYPE_SHIFT_INTEGER:        return _mul_fi(numA, numB);
        case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_fq(numA, numB);
        case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_ff(numA, numB);
        case PMATH_TYPE_SHIFT_MACHINE_FLOAT:  return _mul_mf(numB, numA);
      }
    } break;
    
    case PMATH_TYPE_SHIFT_MACHINE_FLOAT: {
      switch(numB->type_shift){
        case PMATH_TYPE_SHIFT_INTEGER:        return _mul_mi(numA, numB);
        case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_mq(numA, numB);
        case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_mf(numA, numB);
        case PMATH_TYPE_SHIFT_MACHINE_FLOAT:  return _mul_mm(numA, numB);
      }
    } break;
  }
  
  assert("invalid number type" && 0);
  
  return NULL;
}

static void split_factor(
  pmath_t  factor,
  pmath_t *out_base,
  pmath_t *out_num_power,
  pmath_t *out_rest_power
){
  if(pmath_instance_of(factor, PMATH_TYPE_EXPRESSION)
  && pmath_expr_length(factor) == 2){
    pmath_t head = pmath_expr_get_item(factor, 0);
    pmath_unref(head);
    
    if(head == PMATH_SYMBOL_POWER){
      pmath_t exponent = pmath_expr_get_item(factor, 2);
      *out_base = pmath_expr_get_item(factor, 1);
      split_summand(exponent, out_num_power, out_rest_power);
      pmath_unref(exponent);
      return;
    }
  }
  *out_base = pmath_ref(factor);
  *out_num_power = pmath_integer_new_ui(1);
  *out_rest_power = PMATH_UNDEFINED;
}

static void times_2_arg(pmath_t *a, pmath_t *b){
  if(pmath_instance_of(*a, PMATH_TYPE_NUMBER)){
    if(pmath_instance_of(*b, PMATH_TYPE_NUMBER)){
      *a = _mul_nn(*a, *b);
      *a = _pmath_float_exceptions(*a);
      *b = PMATH_UNDEFINED;
      return;
    }

    if(_pmath_is_nonreal_complex(*b)){ // a * (x + yi) = ax + ayi
      pmath_number_t re = _mul_nn(
        pmath_ref(*a),
        pmath_expr_get_item(*b, 1));
      pmath_number_t im = _mul_nn(
        pmath_ref(*a),
        pmath_expr_get_item(*b, 2));
      pmath_unref(*a);
      re = _pmath_float_exceptions(re);
      im = _pmath_float_exceptions(im);
      *a = pmath_expr_set_item(*b, 1, re);
      *a = pmath_expr_set_item(*a, 2, im);
      *b = PMATH_UNDEFINED;
      return;
    }

    if(pmath_equals(*b, _pmath_object_overflow)
    || pmath_equals(*b, _pmath_object_underflow)){
      pmath_unref(*a);
      *a = *b;
      *b = PMATH_UNDEFINED;
      return;
    }
    
    if(pmath_instance_of(*a, PMATH_TYPE_FLOAT)
    && _pmath_is_numeric(*b)){
      *b = pmath_approximate(*b, pmath_precision(pmath_ref(*a)), HUGE_VAL);
      return;
    }
    
    if(pmath_equals(*a, PMATH_NUMBER_ONE)){
      pmath_unref(*a);
      *a = *b;
      *b = PMATH_UNDEFINED;
      return;
    }

    if(pmath_number_sign(*a) == 0){
      pmath_t binfdir = _pmath_directed_infinity_direction(*b);
      if(binfdir){
        pmath_message(PMATH_SYMBOL_INFINITY, "indet", 1,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_TIMES), 2,
            *a,
            *b));
        *a = pmath_ref(PMATH_SYMBOL_INDETERMINATE);
        *b = PMATH_UNDEFINED;
        pmath_unref(binfdir);
        return;
      }
      pmath_unref(*b);
      *b = PMATH_UNDEFINED;
      return;
    }

    if(pmath_equals(*a, PMATH_NUMBER_MINUSONE)
    && pmath_is_expr_of(*b, PMATH_SYMBOL_PLUS)){
      size_t i;

      pmath_unref(*a);
      for(i = 1;i <= pmath_expr_length((pmath_expr_t)*b);i++)
        *b = pmath_expr_set_item((pmath_expr_t)*b, i,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_TIMES), 2,
            pmath_integer_new_si(-1),
            pmath_expr_get_item((pmath_expr_t)*b, i)));

      *a = *b;
      *b = PMATH_UNDEFINED;
      return;
    }
  }
  else if(_pmath_is_nonreal_complex(*a)){
    if(pmath_instance_of(*b, PMATH_TYPE_NUMBER)){ // (x + yi) * b = bx + byi
      pmath_number_t re = _mul_nn(
        (pmath_number_t)pmath_ref(*b),
        (pmath_number_t)pmath_expr_get_item((pmath_expr_t)*a, 1));
      pmath_number_t im = _mul_nn(
        (pmath_number_t)pmath_ref(*b),
        (pmath_number_t)pmath_expr_get_item((pmath_expr_t)*a, 2));
      pmath_unref(*b);
      re = _pmath_float_exceptions(re);
      im = _pmath_float_exceptions(im);
      *a = pmath_expr_set_item(*a, 1, re);
      *a = pmath_expr_set_item(*a, 2, im);
      *b = PMATH_UNDEFINED;
      return;
    }
    
    if(_pmath_is_nonreal_complex(*b)){
      // (u + vi)*(x + yi) = (ux - vy) + (uy + vx)i
      pmath_number_t u = (pmath_number_t)pmath_expr_get_item(*a, 1);
      pmath_number_t v = (pmath_number_t)pmath_expr_get_item(*a, 2);
      pmath_number_t x = (pmath_number_t)pmath_expr_get_item(*b, 1);
      pmath_number_t y = (pmath_number_t)pmath_expr_get_item(*b, 2);

      pmath_unref(*b);
      *a = pmath_expr_set_item(
        *a, 1,
        _add_nn(
          _mul_nn(pmath_ref(u), pmath_ref(x)),
          pmath_number_neg(
            _mul_nn(pmath_ref(v), pmath_ref(y)))));
      *a = pmath_expr_set_item(
        *a, 2,
        _add_nn(
          _mul_nn(pmath_ref(u), pmath_ref(y)),
          _mul_nn(pmath_ref(v), pmath_ref(x))));
      pmath_unref(u);
      pmath_unref(v);
      pmath_unref(x);
      pmath_unref(y);
      *a = _pmath_float_exceptions(*a);
      *b = PMATH_UNDEFINED;
      return;
    }
    
    if(_pmath_is_inexact(*a) && _pmath_is_numeric(*b)){
      *b = pmath_approximate(*b, pmath_precision(pmath_ref(*a)), HUGE_VAL);
      return;
    }
  }
  
  {
    pmath_t infdir = _pmath_directed_infinity_direction(*a);

    if(infdir){
      pmath_unref(*a);
      *a = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_TIMES), 2,
          infdir,
          *b));
      *b = PMATH_UNDEFINED;
      return;
    }

    infdir = _pmath_directed_infinity_direction(*b);
    if(infdir){
      pmath_unref(*b);
      *a = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_TIMES), 2,
          infdir,
          *a));
      *b = PMATH_UNDEFINED;
      return;
    }
  }

  {
    pmath_t baseA;
    pmath_t baseB;
    pmath_number_t numPowerA;
    pmath_number_t numPowerB;
    pmath_t restPowerA;
    pmath_t restPowerB;

    split_factor(*a, &baseA, &numPowerA, &restPowerA);
    split_factor(*b, &baseB, &numPowerB, &restPowerB);

    if(pmath_equals(baseA, baseB) 
    && pmath_equals(restPowerA, restPowerB)
    && (restPowerA != PMATH_UNDEFINED || !pmath_instance_of(baseA, PMATH_TYPE_NUMBER))){
      pmath_unref(*a);
      pmath_unref(baseB);
      pmath_unref(restPowerB);
      pmath_unref(*b);
      *b = PMATH_UNDEFINED;
      
      if(restPowerA == PMATH_UNDEFINED){
        *a = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_POWER), 2,
          baseA,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PLUS), 2,
            numPowerA,
            numPowerB));
      }
      else{
        *a = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_POWER), 2,
          baseA,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_TIMES), 2,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_PLUS), 2,
              numPowerA,
              numPowerB),
            restPowerA));
      }
      
      return;
    }

    pmath_unref(baseA);
    pmath_unref(baseB);
    pmath_unref(numPowerA);
    pmath_unref(numPowerB);
    pmath_unref(restPowerA);
    pmath_unref(restPowerB);
  }

  if((pmath_equals(*a, _pmath_object_overflow)
    && pmath_equals(*b, _pmath_object_underflow))
  || (pmath_equals(*a, _pmath_object_underflow)
    && pmath_equals(*b, _pmath_object_overflow))){
    pmath_unref(*a);
    pmath_unref(*b);
    *a = pmath_ref(PMATH_SYMBOL_INDETERMINATE);
    *b = PMATH_UNDEFINED;
    return;
  }
  
  if((pmath_equals(*a, _pmath_object_overflow)
   || pmath_equals(*a, _pmath_object_underflow))
  && _pmath_is_numeric(*b)){
    if(pmath_equals(*a, _pmath_object_overflow)
    || pmath_equals(*a, _pmath_object_underflow))
    pmath_unref(*b);
    *b = PMATH_UNDEFINED;
    return;
  }

  if((pmath_equals(*b, _pmath_object_overflow)
   || pmath_equals(*b, _pmath_object_underflow))
  && _pmath_is_numeric(*a)){
    pmath_unref(*a);
    *a = *b;
    *b = PMATH_UNDEFINED;
    return;
  }
}

PMATH_PRIVATE pmath_t builtin_times(pmath_expr_t expr){
  size_t ia, ib;
  const size_t elen = pmath_expr_length(expr);

  if(elen == 0){
    pmath_unref(expr);
    return pmath_integer_new_ui(1);
  }
  
  if(elen == 1){
    pmath_t item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return item;
  }

  ia = 1;
  while(ia < elen){
    pmath_t a = pmath_expr_get_item(expr, ia);
    if(a != PMATH_UNDEFINED){
      ib = ia + 1;
      while(ib <= elen){
        pmath_t b = pmath_expr_get_item(expr, ib);
        if(b != PMATH_UNDEFINED){
          times_2_arg(&a, &b);
          expr = pmath_expr_set_item(expr, ia, pmath_ref(a));
          expr = pmath_expr_set_item(expr, ib, b);
        }
        ++ib;
      }
      pmath_unref(a);
    }
    ++ia;
  }
  
  return _pmath_expr_shrink_associative(expr, PMATH_UNDEFINED);
}
