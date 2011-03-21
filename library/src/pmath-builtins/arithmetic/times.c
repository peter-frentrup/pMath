#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/number-theory-private.h>

static pmath_rational_t _mul_qi(
  pmath_quotient_t quotA, // will be freed. not PMATH_NULL!
  pmath_integer_t  intB   // will be freed. not PMATH_NULL!
){ // u/v * w = (u*w)/v
  pmath_integer_t numerator   = pmath_ref(PMATH_QUOT_NUM(quotA));
  pmath_integer_t denominator = pmath_ref(PMATH_QUOT_DEN(quotA));
  pmath_unref(quotA);

  return pmath_rational_new(
    _mul_ii(numerator, intB),
    denominator);
}

static pmath_rational_t _mul_qq(
  pmath_quotient_t quotA, // will be freed. not PMATH_NULL!
  pmath_quotient_t quotB  // will be freed. not PMATH_NULL!
){ // u/v * w/x = (u*w)/(v*x)
  pmath_integer_t numeratorA   = pmath_ref(PMATH_QUOT_NUM(quotA));
  pmath_integer_t denominatorA = pmath_ref(PMATH_QUOT_DEN(quotA));

  pmath_integer_t numeratorB   = pmath_ref(PMATH_QUOT_NUM(quotB));
  pmath_integer_t denominatorB = pmath_ref(PMATH_QUOT_DEN(quotB));

  pmath_unref(quotA);
  pmath_unref(quotB);

  return pmath_rational_new(
    _mul_ii(numeratorA,   numeratorB),
    _mul_ii(denominatorA, denominatorB));
}

static pmath_mpfloat_t _mul_fi(
  pmath_mpfloat_t floatA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB    // will be freed. not PMATH_NULL!
){
  pmath_mpfloat_t result;
  double fprec;
  long   expB;
  mpfr_prec_t prec;
  
  if(pmath_is_int32(intB)){
    if(PMATH_AS_INT32(intB) == 0){
      pmath_unref(floatA);
      return intB;
    }
    
    intB = _pmath_create_mp_int(PMATH_AS_INT32(intB));
    
    if(pmath_is_null(intB)){
      pmath_unref(floatA);
      return intB;
    }
  }
  
  assert(pmath_is_mpint(intB));
  
  fprec = pmath_precision(pmath_ref(floatA));
  fprec+= log2(fabs(mpz_get_d_2exp(&expB, PMATH_AS_MPZ(intB))));
  fprec+= expB;
  
  if(fprec < MPFR_PREC_MIN)
    prec = MPFR_PREC_MIN;
  else if(fprec > PMATH_MP_PREC_MAX)
    prec = PMATH_MP_PREC_MAX;
  else
    prec = (mpfr_prec_t)ceil(fprec);
  
  result = _pmath_create_mp_float(prec);
  
  if(pmath_is_null(result)){
    pmath_unref(floatA);
    pmath_unref(intB);
    return PMATH_NULL;
  }
  
  // dxy = y dx + x dy = y dx   (dy = 0)
  mpfr_mul_z(
    PMATH_AS_MP_ERROR(result), 
    PMATH_AS_MP_ERROR(floatA),
    PMATH_AS_MPZ(intB),
    MPFR_RNDA);
  
  mpfr_abs(PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(result), MPFR_RNDN);
  
  mpfr_mul_z(
    PMATH_AS_MP_VALUE(result),
    PMATH_AS_MP_VALUE(floatA),
    PMATH_AS_MPZ(intB),
    MPFR_RNDN);
  
  //_pmath_mp_float_normalize(result);

  pmath_unref(floatA);
  pmath_unref(intB);
  return result;
}

  static pmath_mpfloat_t _div_fi(
    pmath_mpfloat_t floatA, // will be freed. not PMATH_NULL!
    pmath_integer_t intB    // wont be freed. not PMATH_NULL! not zero!
  ){
    pmath_mpfloat_t result;
    
    result = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(floatA)));
    
    if(pmath_is_null(result)){
      pmath_unref(floatA);
      return PMATH_NULL;
    }
    
    // d(x/y) = 1/y dx + x d(1/y) = 1/y dx   (d(1/y) = 0)
    if(pmath_is_int32(intB)){
      mpfr_div_si(
        PMATH_AS_MP_ERROR(result), 
        PMATH_AS_MP_ERROR(floatA),
        PMATH_AS_INT32(intB),
        MPFR_RNDA);
      
      mpfr_abs(PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(result), MPFR_RNDU);
      
      mpfr_div_si(
        PMATH_AS_MP_VALUE(result),
        PMATH_AS_MP_VALUE(floatA),
        PMATH_AS_INT32(intB),
        MPFR_RNDN);
    }
    else{
      mpfr_div_z(
        PMATH_AS_MP_ERROR(result), 
        PMATH_AS_MP_ERROR(floatA),
        PMATH_AS_MPZ(intB),
        MPFR_RNDA);
      
      mpfr_abs(PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(result), MPFR_RNDU);
      
      mpfr_div_z(
        PMATH_AS_MP_VALUE(result),
        PMATH_AS_MP_VALUE(floatA),
        PMATH_AS_MPZ(intB),
        MPFR_RNDN);
    }
    
    _pmath_mp_float_normalize(result);

    pmath_unref(floatA);
    return result;
  }

static pmath_mpfloat_t _mul_fq(
  pmath_mpfloat_t  floatA, // will be freed. not PMATH_NULL!
  pmath_quotient_t quotB   // will be freed. not PMATH_NULL!
){
  floatA = _mul_fi(floatA, pmath_ref(PMATH_QUOT_NUM(quotB)));
  
  if(pmath_is_mpfloat(floatA)){
    floatA = _div_fi(floatA, PMATH_QUOT_DEN(quotB));
    pmath_unref(quotB);
  }
  
  return floatA;
}

static pmath_mpfloat_t _mul_ff(
  pmath_mpfloat_t floatA, // will be freed. not PMATH_NULL!
  pmath_mpfloat_t floatB  // will be freed. not PMATH_NULL!
){
  pmath_mpfloat_t result;
  pmath_mpfloat_t tmp_err;
  double fprec;
  mpfr_prec_t prec;
  
  fprec = pmath_precision(pmath_ref(floatA)) 
        + pmath_precision(pmath_ref(floatB));
  
  if(fprec < MPFR_PREC_MIN)
    prec = MPFR_PREC_MIN;
  else if(fprec > PMATH_MP_PREC_MAX)
    prec = PMATH_MP_PREC_MAX;
  else
    prec = (mpfr_prec_t)ceil(fprec);
  
  result = _pmath_create_mp_float(prec);
  tmp_err = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
  
  if(pmath_is_null(result) || pmath_is_null(tmp_err)){
    pmath_unref(result);
    pmath_unref(tmp_err);
    pmath_unref(floatA);
    pmath_unref(floatB);
    return PMATH_NULL;
  }
  
  // dxy = y dx + x dy
  
  mpfr_mul(
    PMATH_AS_MP_VALUE(tmp_err),
    PMATH_AS_MP_VALUE(floatB),
    PMATH_AS_MP_ERROR(floatA),
    MPFR_RNDA);
  
  mpfr_abs(PMATH_AS_MP_VALUE(tmp_err), PMATH_AS_MP_VALUE(tmp_err), MPFR_RNDU);
  
  mpfr_mul(
    PMATH_AS_MP_ERROR(tmp_err),
    PMATH_AS_MP_VALUE(floatA),
    PMATH_AS_MP_ERROR(floatB),
    MPFR_RNDA);
  
  mpfr_abs(PMATH_AS_MP_ERROR(tmp_err), PMATH_AS_MP_ERROR(tmp_err), MPFR_RNDU);
  
  mpfr_add(
    PMATH_AS_MP_ERROR(result),
    PMATH_AS_MP_VALUE(tmp_err),
    PMATH_AS_MP_ERROR(tmp_err),
    MPFR_RNDU);
  
  mpfr_fma(
    PMATH_AS_MP_ERROR(result),
    PMATH_AS_MP_ERROR(floatA),
    PMATH_AS_MP_ERROR(floatB),
    PMATH_AS_MP_ERROR(result),
    MPFR_RNDU);
  
  mpfr_mul(
    PMATH_AS_MP_VALUE(result),
    PMATH_AS_MP_VALUE(floatA),
    PMATH_AS_MP_VALUE(floatB),
    MPFR_RNDN);
  
  _pmath_mp_float_normalize(result);
  
  pmath_unref(floatA);
  pmath_unref(floatB);
  pmath_unref(tmp_err);
  return result;
}

static pmath_t _mul_mi(
  pmath_float_t   floatA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB    // will be freed. not PMATH_NULL!
){
  double d = PMATH_AS_DOUBLE(floatA);
  
  if(pmath_is_int32(intB)) 
    d*= PMATH_AS_INT32(intB);
  else
    d*= mpz_get_d(PMATH_AS_MPZ(intB));
  
  if(!isfinite(d))
    return _mul_nn(_pmath_convert_to_mp_float(floatA), intB);
  
  pmath_unref(intB);
  pmath_unref(floatA);
  return PMATH_FROM_DOUBLE(d);
}

static pmath_t _mul_mq(
  pmath_float_t    floatA, // will be freed. not PMATH_NULL!
  pmath_quotient_t quotB   // will be freed. not PMATH_NULL!
){
  pmath_integer_t numB = PMATH_QUOT_NUM(quotB);
  pmath_integer_t denB = PMATH_QUOT_DEN(quotB);
  double d;
  
  if(pmath_is_int32(numB)){
    d = PMATH_AS_INT32(numB);
  }
  else
    d = mpz_get_d(PMATH_AS_MPZ(numB));
    
  if(pmath_is_int32(denB))
    d/= PMATH_AS_INT32(denB);
  else
    d/= mpz_get_d(PMATH_AS_MPZ(denB));
  
  d*= PMATH_AS_DOUBLE(floatA);
  
  if(!isfinite(d))
    return _mul_nn(_pmath_convert_to_mp_float(floatA), quotB);
  
  pmath_unref(quotB);
  pmath_unref(floatA);
  return PMATH_FROM_DOUBLE(d);
}

static pmath_t _mul_mf(
  pmath_float_t   floatA, // will be freed. not PMATH_NULL!
  pmath_mpfloat_t floatB  // will be freed. not PMATH_NULL!
){
  double d = PMATH_AS_DOUBLE(floatA) * mpfr_get_d(PMATH_AS_MP_VALUE(floatB), MPFR_RNDN);
  
  if(!isfinite(d))
    return _mul_nn(_pmath_convert_to_mp_float(floatA), floatB);
  
  pmath_unref(floatB);
  pmath_unref(floatA);
  return PMATH_FROM_DOUBLE(d);
}

static pmath_t _mul_mm(
  pmath_float_t  floatA, // will be freed. not PMATH_NULL!
  pmath_float_t  floatB  // will be freed. not PMATH_NULL!
){
  double d = PMATH_AS_DOUBLE(floatA) * PMATH_AS_DOUBLE(floatB);
          
  if(!isfinite(d))
    return _mul_nn(
      _pmath_convert_to_mp_float(floatA), 
      _pmath_convert_to_mp_float(floatB));
  
  pmath_unref(floatA);
  pmath_unref(floatB);
  
  return PMATH_FROM_DOUBLE(d);
}

PMATH_PRIVATE pmath_number_t _mul_nn(
  pmath_number_t numA, // will be freed.
  pmath_number_t numB  // will be freed.
){
  if(pmath_is_null(numA) || pmath_is_null(numB)){
    pmath_unref(numA);
    pmath_unref(numB);
    return PMATH_NULL;
  }
  
  if(pmath_is_double(numA)){
    if(pmath_is_double(numB))
      return _mul_mm(numA, numB);
      
    if(pmath_is_int32(numB))
      return _mul_mi(numA, numB);
    
    assert(pmath_is_pointer(numB));
    
    switch(PMATH_AS_PTR(numB)->type_shift){
      case PMATH_TYPE_SHIFT_MP_INT:         return _mul_mi(numA, numB);
      case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_mq(numA, numB);
      case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_mf(numA, numB);
    }
    
    assert("invalid number type" && 0);
    return PMATH_NULL;
  }
  
  if(pmath_is_double(numB))
    return _mul_nn(numB, numA);
  
  if(pmath_is_int32(numA)){
    if(pmath_is_int32(numB))
      return _mul_ii(numB, numA);
    
    assert(pmath_is_pointer(numB));
    
    switch(PMATH_AS_PTR(numB)->type_shift){
      case PMATH_TYPE_SHIFT_MP_INT:         return _mul_ii(numB, numA);
      case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_qi(numB, numA);
      case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_fi(numB, numA);
    }
    
    assert("invalid number type" && 0);
    return PMATH_NULL;
  }
  
  if(pmath_is_int32(numB))
    return _mul_nn(numB, numA);
  
  assert(pmath_is_pointer(numA));
  
  switch(PMATH_AS_PTR(numA)->type_shift){
    case PMATH_TYPE_SHIFT_MP_INT: {
      switch(PMATH_AS_PTR(numB)->type_shift){
        case PMATH_TYPE_SHIFT_MP_INT:         return _mul_ii(numA, numB);
        case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_qi(numB, numA);
        case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_fi(numB, numA);
      }
    } break;
    
    case PMATH_TYPE_SHIFT_QUOTIENT: {
      switch(PMATH_AS_PTR(numB)->type_shift){
        case PMATH_TYPE_SHIFT_MP_INT:         return _mul_qi(numA, numB);
        case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_qq(numA, numB);
        case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_fq(numB, numA);
      }
    } break;
    
    case PMATH_TYPE_SHIFT_MP_FLOAT: {
      switch(PMATH_AS_PTR(numB)->type_shift){
        case PMATH_TYPE_SHIFT_MP_INT:         return _mul_fi(numA, numB);
        case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_fq(numA, numB);
        case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_ff(numA, numB);
      }
    } break;
  }
  
  assert("invalid number type" && 0);
  
  return PMATH_NULL;
}

static void split_factor(
  pmath_t  factor,
  pmath_t *out_base,
  pmath_t *out_num_power,
  pmath_t *out_rest_power
){
  if(pmath_is_expr(factor) && pmath_expr_length(factor) == 2){
    pmath_t head = pmath_expr_get_item(factor, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_SYMBOL_POWER)){
      pmath_t exponent = pmath_expr_get_item(factor, 2);
      *out_base = pmath_expr_get_item(factor, 1);
      split_summand(exponent, out_num_power, out_rest_power);
      pmath_unref(exponent);
      return;
    }
  }
  *out_base = pmath_ref(factor);
  *out_num_power  = PMATH_FROM_INT32(1);
  *out_rest_power = PMATH_UNDEFINED;
}

static void times_2_arg(pmath_t *a, pmath_t *b){
  if(pmath_is_number(*a)){
    if(pmath_is_number(*b)){
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
    
    if(pmath_is_float(*a) && _pmath_is_numeric(*b)){
      *b = pmath_approximate(*b, pmath_precision(pmath_ref(*a)), HUGE_VAL);
      return;
    }
    
    if(pmath_equals(*a, PMATH_FROM_INT32(1))){
      pmath_unref(*a);
      *a = *b;
      *b = PMATH_UNDEFINED;
      return;
    }

    if(pmath_number_sign(*a) == 0){
      pmath_t binfdir = _pmath_directed_infinity_direction(*b);
      
      if(!pmath_is_null(binfdir)){
        pmath_message(PMATH_SYMBOL_INFINITY, "indet", 1,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_TIMES), 2,
            *a,
            *b));
        *a = pmath_ref(PMATH_SYMBOL_UNDEFINED);
        *b = PMATH_UNDEFINED;
        pmath_unref(binfdir);
        return;
      }
      
      pmath_unref(*b);
      *b = PMATH_UNDEFINED;
      return;
    }

    if(pmath_equals(*a, PMATH_FROM_INT32(-1))
    && pmath_is_expr_of(*b, PMATH_SYMBOL_PLUS)){
      size_t i;

      pmath_unref(*a);
      for(i = 1;i <= pmath_expr_length(*b);i++)
        *b = pmath_expr_set_item(
          *b, i,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_TIMES), 2,
            PMATH_FROM_INT32(-1),
            pmath_expr_get_item(*b, i)));

      *a = *b;
      *b = PMATH_UNDEFINED;
      return;
    }
  }
  else if(_pmath_is_nonreal_complex(*a)){
    if(pmath_is_number(*b)){ // (x + yi) * b = bx + byi
      pmath_number_t re = _mul_nn(pmath_ref(*b), pmath_expr_get_item(*a, 1));
      pmath_number_t im = _mul_nn(pmath_ref(*b), pmath_expr_get_item(*a, 2));
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
      pmath_number_t u = pmath_expr_get_item(*a, 1);
      pmath_number_t v = pmath_expr_get_item(*a, 2);
      pmath_number_t x = pmath_expr_get_item(*b, 1);
      pmath_number_t y = pmath_expr_get_item(*b, 2);

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

    if(!pmath_is_null(infdir)){
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
    if(!pmath_is_null(infdir)){
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
    && (!pmath_same(restPowerA, PMATH_UNDEFINED) || !pmath_is_number(baseA))){
      pmath_unref(*a);
      pmath_unref(baseB);
      pmath_unref(restPowerB);
      pmath_unref(*b);
      *b = PMATH_UNDEFINED;
      
      if(pmath_same(restPowerA, PMATH_UNDEFINED)){
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
    *a = pmath_ref(PMATH_SYMBOL_UNDEFINED);
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
    return PMATH_FROM_INT32(1);
  }
  
  if(elen == 1){
    pmath_t item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return item;
  }

  ia = 1;
  while(ia < elen){
    pmath_t a = pmath_expr_get_item(expr, ia);
    if(!pmath_same(a, PMATH_UNDEFINED)){
      ib = ia + 1;
      while(ib <= elen){
        pmath_t b = pmath_expr_get_item(expr, ib);
        if(!pmath_same(b, PMATH_UNDEFINED)){
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
