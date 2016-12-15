#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/intervals-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/number-theory-private.h>

#define MAX(x, y) ((x) >= (y) ? (x) : (y))


static pmath_rational_t _mul_qi(
  pmath_quotient_t quotA, // will be freed. not PMATH_NULL!
  pmath_integer_t  intB   // will be freed. not PMATH_NULL!
) { // u/v * w = (u*w)/v
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
) { // u/v * w/x = (u*w)/(v*x)
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
) {
  pmath_mpfloat_t result;
  
  assert(pmath_is_mpfloat(floatA));
  
  result = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(floatA)));
  if(pmath_is_null(result)) {
    pmath_unref(floatA);
    pmath_unref(intB);
    return PMATH_NULL;
  }
  
  if(pmath_is_int32(intB)) {
    if(PMATH_AS_INT32(intB) == 0) {
      pmath_unref(floatA);
      return intB;
    }
    
    mpfr_mul_si(
      PMATH_AS_MP_VALUE(result),
      PMATH_AS_MP_VALUE(floatA),
      PMATH_AS_INT32(intB),
      _pmath_current_rounding_mode());
      
    pmath_unref(floatA);
    return _pmath_float_exceptions(result);
  }
  
  assert(pmath_is_mpint(intB));
  
  mpfr_mul_z(
    PMATH_AS_MP_VALUE(result),
    PMATH_AS_MP_VALUE(floatA),
    PMATH_AS_MPZ(intB),
    _pmath_current_rounding_mode());
    
  pmath_unref(floatA);
  pmath_unref(intB);
  return _pmath_float_exceptions(result);
}

static pmath_mpfloat_t _mul_fq(
  pmath_mpfloat_t  floatA, // will be freed. not PMATH_NULL!
  pmath_quotient_t quotB   // will be freed. not PMATH_NULL!
) {
  pmath_mpfloat_t result;
  mpq_t mpQuotB;
  
  assert(pmath_is_mpfloat(floatA));
  assert(pmath_is_quotient(quotB));
  
  result = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(floatA)));
  if(pmath_is_null(result)) {
    pmath_unref(result);
    pmath_unref(floatA);
    pmath_unref(quotB);
    return PMATH_NULL;
  }
  
  mpq_init(mpQuotB);
  
  if(pmath_is_int32(PMATH_QUOT_NUM(quotB))) {
    mpz_set_si(mpq_numref(mpQuotB), PMATH_AS_INT32(PMATH_QUOT_NUM(quotB)));
  }
  else {
    assert(pmath_is_mpint(PMATH_QUOT_NUM(quotB)));
    
    mpz_set(mpq_numref(mpQuotB), PMATH_AS_MPZ(PMATH_QUOT_NUM(quotB)));
  }
  
  if(pmath_is_int32(PMATH_QUOT_DEN(quotB))) {
    mpz_set_si(mpq_denref(mpQuotB), PMATH_AS_INT32(PMATH_QUOT_DEN(quotB)));
  }
  else {
    assert(pmath_is_mpint(PMATH_QUOT_DEN(quotB)));
    
    mpz_set(mpq_denref(mpQuotB), PMATH_AS_MPZ(PMATH_QUOT_DEN(quotB)));
  }
  
  mpfr_mul_q(
    PMATH_AS_MP_VALUE(result),
    PMATH_AS_MP_VALUE(floatA),
    mpQuotB,
    _pmath_current_rounding_mode());
    
  pmath_unref(floatA);
  pmath_unref(quotB);
  mpq_clear(mpQuotB);
  
  return _pmath_float_exceptions(result);
}

static pmath_mpfloat_t _mul_ff(
  pmath_mpfloat_t floatA, // will be freed. not PMATH_NULL!
  pmath_mpfloat_t floatB  // will be freed. not PMATH_NULL!
) {
  pmath_mpfloat_t result;
  mpfr_prec_t prec;
  
  assert(pmath_is_mpfloat(floatA));
  assert(pmath_is_mpfloat(floatB));
  
  prec = min_prec(mpfr_get_prec(PMATH_AS_MP_VALUE(floatA)),
                  mpfr_get_prec(PMATH_AS_MP_VALUE(floatB)));
                  
  result = _pmath_create_mp_float(prec);
  if(pmath_is_null(result)) {
    pmath_unref(floatA);
    pmath_unref(floatB);
    return PMATH_NULL;
  }
  
  mpfr_mul(
    PMATH_AS_MP_VALUE(result),
    PMATH_AS_MP_VALUE(floatA),
    PMATH_AS_MP_VALUE(floatB),
    _pmath_current_rounding_mode());
    
  pmath_unref(floatA);
  pmath_unref(floatB);
  return _pmath_float_exceptions(result);
}

static pmath_t _mul_mi(
  pmath_float_t   floatA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB    // will be freed. not PMATH_NULL!
) {
  double d = PMATH_AS_DOUBLE(floatA);
  
  if(pmath_is_int32(intB))
    d *= PMATH_AS_INT32(intB);
  else
    d *= mpz_get_d(PMATH_AS_MPZ(intB));
    
  if(!isfinite(d))
    return _mul_nn(_pmath_convert_to_mp_float(floatA), intB);
    
  pmath_unref(intB);
  pmath_unref(floatA);
  return PMATH_FROM_DOUBLE(d);
}

static pmath_t _mul_mq(
  pmath_float_t    floatA, // will be freed. not PMATH_NULL!
  pmath_quotient_t quotB   // will be freed. not PMATH_NULL!
) {
  pmath_integer_t numB = PMATH_QUOT_NUM(quotB);
  pmath_integer_t denB = PMATH_QUOT_DEN(quotB);
  double d;
  
  if(pmath_is_int32(numB)) {
    d = PMATH_AS_INT32(numB);
  }
  else
    d = mpz_get_d(PMATH_AS_MPZ(numB));
    
  if(pmath_is_int32(denB))
    d /= PMATH_AS_INT32(denB);
  else
    d /= mpz_get_d(PMATH_AS_MPZ(denB));
    
  d *= PMATH_AS_DOUBLE(floatA);
  
  if(!isfinite(d))
    return _mul_nn(_pmath_convert_to_mp_float(floatA), quotB);
    
  pmath_unref(quotB);
  pmath_unref(floatA);
  return PMATH_FROM_DOUBLE(d);
}

static pmath_t _mul_mf(
  pmath_float_t   floatA, // will be freed. not PMATH_NULL!
  pmath_mpfloat_t floatB  // will be freed. not PMATH_NULL!
) {
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
) {
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
) {
  if(pmath_is_null(numA) || pmath_is_null(numB)) {
    pmath_unref(numA);
    pmath_unref(numB);
    return PMATH_NULL;
  }
  
  if(pmath_is_double(numA)) {
    if(pmath_is_double(numB))
      return _mul_mm(numA, numB);
      
    if(pmath_is_int32(numB))
      return _mul_mi(numA, numB);
      
    assert(pmath_is_pointer(numB));
    
    switch(PMATH_AS_PTR(numB)->type_shift) {
      case PMATH_TYPE_SHIFT_MP_INT:         return _mul_mi(numA, numB);
      case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_mq(numA, numB);
      case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_mf(numA, numB);
    }
    
    assert("invalid number type" && 0);
    return PMATH_NULL;
  }
  
  if(pmath_is_double(numB))
    return _mul_nn(numB, numA);
    
  if(pmath_is_int32(numA)) {
    if(pmath_is_int32(numB))
      return _mul_ii(numB, numA);
      
    assert(pmath_is_pointer(numB));
    
    switch(PMATH_AS_PTR(numB)->type_shift) {
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
  
  switch(PMATH_AS_PTR(numA)->type_shift) {
    case PMATH_TYPE_SHIFT_MP_INT: {
        switch(PMATH_AS_PTR(numB)->type_shift) {
          case PMATH_TYPE_SHIFT_MP_INT:         return _mul_ii(numA, numB);
          case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_qi(numB, numA);
          case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_fi(numB, numA);
        }
      } break;
      
    case PMATH_TYPE_SHIFT_QUOTIENT: {
        switch(PMATH_AS_PTR(numB)->type_shift) {
          case PMATH_TYPE_SHIFT_MP_INT:         return _mul_qi(numA, numB);
          case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_qq(numA, numB);
          case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_fq(numB, numA);
        }
      } break;
      
    case PMATH_TYPE_SHIFT_MP_FLOAT: {
        switch(PMATH_AS_PTR(numB)->type_shift) {
          case PMATH_TYPE_SHIFT_MP_INT:         return _mul_fi(numA, numB);
          case PMATH_TYPE_SHIFT_QUOTIENT:       return _mul_fq(numA, numB);
          case PMATH_TYPE_SHIFT_MP_FLOAT:       return _mul_ff(numA, numB);
        }
      } break;
  }
  
  assert("invalid number type" && 0);
  
  return PMATH_NULL;
}


static pmath_interval_t _mul_RR(pmath_interval_t intervalA, pmath_interval_t intervalB) {
  pmath_interval_t result;
  mp_prec_t aprec;
  mp_prec_t bprec;
  
  assert(pmath_is_interval(intervalA));
  assert(pmath_is_interval(intervalB));
  
  aprec = mpfi_get_prec(PMATH_AS_MP_INTERVAL(intervalA));
  bprec = mpfi_get_prec(PMATH_AS_MP_INTERVAL(intervalB));
  result = _pmath_create_interval_for_result_with_prec(intervalA, MAX(aprec, bprec));
  if(!pmath_is_null(result)) {
    mpfi_mul(PMATH_AS_MP_INTERVAL(result), PMATH_AS_MP_INTERVAL(intervalA), PMATH_AS_MP_INTERVAL(intervalB));
  }
  pmath_unref(intervalA);
  pmath_unref(intervalB);
  return result;
}

static pmath_interval_t _mul_Rs(pmath_interval_t intervalA, pmath_integer_t int32B) {
  pmath_interval_t result;
  
  assert(pmath_is_interval(intervalA));
  assert(pmath_is_int32(int32B));
  
  result = _pmath_create_interval_for_result(intervalA);
  if(!pmath_is_null(result)) {
    mpfi_mul_si(PMATH_AS_MP_INTERVAL(result), PMATH_AS_MP_INTERVAL(intervalA), PMATH_AS_INT32(int32B));
  }
  pmath_unref(intervalA);
  //pmath_unref(int32B);
  return result;
}

static pmath_interval_t _mul_Ri(pmath_interval_t intervalA, pmath_integer_t mpintB) {
  pmath_interval_t result;
  
  assert(pmath_is_interval(intervalA));
  assert(pmath_is_mpint(mpintB));
  
  result = _pmath_create_interval_for_result(intervalA);
  if(!pmath_is_null(result)) {
    mpfi_mul_z(PMATH_AS_MP_INTERVAL(result), PMATH_AS_MP_INTERVAL(intervalA), PMATH_AS_MPZ(mpintB));
  }
  pmath_unref(intervalA);
  pmath_unref(mpintB);
  return result;
}

static pmath_interval_t _mul_Rq(pmath_interval_t intervalA, pmath_quotient_t quotB) {
  pmath_interval_t result;
  mpq_t mpQuotB;
  
  assert(pmath_is_interval(intervalA));
  assert(pmath_is_quotient(quotB));
  
  mpq_init(mpQuotB);
  
  if(pmath_is_int32(PMATH_QUOT_NUM(quotB))) {
    mpz_set_si(mpq_numref(mpQuotB), PMATH_AS_INT32(PMATH_QUOT_NUM(quotB)));
  }
  else {
    assert(pmath_is_mpint(PMATH_QUOT_NUM(quotB)));
    mpz_set(mpq_numref(mpQuotB), PMATH_AS_MPZ(PMATH_QUOT_NUM(quotB)));
  }
  
  if(pmath_is_int32(PMATH_QUOT_DEN(quotB))) {
    mpz_set_si(mpq_denref(mpQuotB), PMATH_AS_INT32(PMATH_QUOT_DEN(quotB)));
  }
  else {
    assert(pmath_is_mpint(PMATH_QUOT_DEN(quotB)));
    mpz_set(mpq_denref(mpQuotB), PMATH_AS_MPZ(PMATH_QUOT_DEN(quotB)));
  }
  
  result = _pmath_create_interval_for_result(intervalA);
  if(!pmath_is_null(result)) {
    mpfi_mul_q(PMATH_AS_MP_INTERVAL(result), PMATH_AS_MP_INTERVAL(intervalA), mpQuotB);
  }
  pmath_unref(intervalA);
  pmath_unref(quotB);
  mpq_clear(mpQuotB);
  return result;
}

static pmath_interval_t _mul_Rf(pmath_interval_t intervalA, pmath_mpfloat_t floatB) {
  pmath_interval_t result;
  
  assert(pmath_is_interval(intervalA));
  assert(pmath_is_mpfloat(floatB));
  
  result = _pmath_create_interval_for_result(intervalA);
  if(!pmath_is_null(result)) {
    mpfi_mul_fr(PMATH_AS_MP_INTERVAL(result), PMATH_AS_MP_INTERVAL(intervalA), PMATH_AS_MP_VALUE(floatB));
  }
  pmath_unref(intervalA);
  pmath_unref(floatB);
  return result;
}

static pmath_interval_t _mul_Rm(pmath_interval_t intervalA, pmath_float_t doubleB) {
  pmath_interval_t result;
  
  assert(pmath_is_interval(intervalA));
  assert(pmath_is_double(doubleB));
  
  result = _pmath_create_interval_for_result(intervalA);
  if(!pmath_is_null(result)) {
    mpfi_mul_d(PMATH_AS_MP_INTERVAL(result), PMATH_AS_MP_INTERVAL(intervalA), PMATH_AS_DOUBLE(doubleB));
  }
  pmath_unref(intervalA);
  //pmath_unref(doubleB);
  return result;
}

static void split_factor(
  pmath_t  factor,
  pmath_t *out_base,
  pmath_t *out_num_power,
  pmath_t *out_rest_power
) {
  if(pmath_is_expr_of_len(factor, PMATH_SYMBOL_POWER, 2)) {
    pmath_t exponent = pmath_expr_get_item(factor, 2);
    *out_base        = pmath_expr_get_item(factor, 1);
    _pmath_split_summand(exponent, out_num_power, out_rest_power);
    pmath_unref(exponent);
    return;
  }
  *out_base = pmath_ref(factor);
  *out_num_power  = INT(1);
  *out_rest_power = PMATH_UNDEFINED;
}

static pmath_bool_t times_2_arg(pmath_t *a, pmath_t *b);

static pmath_bool_t try_multiply_interval_to(pmath_interval_t *a, pmath_t *b) {
  assert(pmath_is_interval(*a));
  
  if(pmath_is_interval(*b)) {
    *a = _mul_RR(*a, *b);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  if(pmath_is_int32(*b)) {
    *a = _mul_Rs(*a, *b);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  if(pmath_is_mpint(*b)) {
    *a = _mul_Ri(*a, *b);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  if(pmath_is_quotient(*b)) {
    *a = _mul_Rq(*a, *b);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  if(pmath_is_mpfloat(*b)) {
    *a = _mul_Rf(*a, *b);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  if(pmath_is_double(*b)) {
    *a = _mul_Rm(*a, *b);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  return FALSE;
}

static pmath_bool_t try_multiply_real_number_to(pmath_number_t *a, pmath_t *b) {
  assert(pmath_is_number(*a));
  
  if(pmath_is_number(*b)) {
    *a = _mul_nn(*a, *b);
    *a = _pmath_float_exceptions(*a);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  if(_pmath_is_nonreal_complex(*b)) { // a * (x + yi) = ax + ayi
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
    return TRUE;
  }
  
  if( pmath_equals(*b, _pmath_object_overflow) ||
      pmath_equals(*b, _pmath_object_underflow))
  {
    pmath_unref(*a);
    *a = *b;
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  if(pmath_is_float(*a) && pmath_is_numeric(*b)) {
    *b = pmath_set_precision(*b, pmath_precision(pmath_ref(*a)));
    return TRUE;
  }
  
  if(pmath_equals(*a, INT(1))) {
    pmath_unref(*a);
    *a = *b;
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  if(pmath_number_sign(*a) == 0) {
    pmath_t binfdir = _pmath_directed_infinity_direction(*b);
    
    if(!pmath_is_null(binfdir)) {
      pmath_message(PMATH_SYMBOL_INFINITY, "indet", 1, TIMES(*a, *b));
      *a = pmath_ref(PMATH_SYMBOL_UNDEFINED);
      *b = PMATH_UNDEFINED;
      pmath_unref(binfdir);
      return TRUE;
    }
    
    pmath_unref(*b);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  if( pmath_equals(*a, INT(-1)) &&
      pmath_is_expr_of(*b, PMATH_SYMBOL_PLUS))
  {
    size_t i;
    
    pmath_unref(*a);
    for(i = 1; i <= pmath_expr_length(*b); i++) {
      pmath_t bi = pmath_expr_extract_item(*b, i);
      
      *b = pmath_expr_set_item(*b, i, NEG(bi));
    }
    
    *a = *b;
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  return FALSE;
}

static pmath_bool_t try_multiply_nonreal_complex_to(pmath_t *a, pmath_t *b) {
  assert(_pmath_is_nonreal_complex(*a));
  
  if(pmath_is_number(*b)) { // (x + yi) * b = bx + byi
    pmath_number_t re = _mul_nn(pmath_ref(*b), pmath_expr_get_item(*a, 1));
    pmath_number_t im = _mul_nn(pmath_ref(*b), pmath_expr_get_item(*a, 2));
    pmath_unref(*b);
    re = _pmath_float_exceptions(re);
    im = _pmath_float_exceptions(im);
    *a = pmath_expr_set_item(*a, 1, re);
    *a = pmath_expr_set_item(*a, 2, im);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  if(_pmath_is_nonreal_complex(*b)) {
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
    return TRUE;
  }
  
  if(_pmath_is_inexact(*a) && pmath_is_numeric(*b)) {
    *b = pmath_set_precision(*b, pmath_precision(pmath_ref(*a)));
    return TRUE;
  }
  
  return FALSE;
}

static pmath_bool_t try_multiply_infinities(pmath_t *a, pmath_t *b) {
  pmath_t infdir = _pmath_directed_infinity_direction(*a);
  
  if(!pmath_is_null(infdir)) {
    pmath_unref(*a);
    *a = pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
           pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_TIMES), 2,
             infdir,
             *b));
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  infdir = _pmath_directed_infinity_direction(*b);
  if(!pmath_is_null(infdir)) {
    pmath_unref(*b);
    *a = pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
           pmath_expr_new_extended(
             pmath_ref(PMATH_SYMBOL_TIMES), 2,
             infdir,
             *a));
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  return FALSE;
}

// base wont be freed
// exponent wont be freed
static pmath_t expand_product_power(pmath_t base, pmath_t exponent) {
  if(pmath_same(exponent, INT(1))) {
    if(pmath_is_expr_of(base, PMATH_SYMBOL_TIMES))
      return pmath_ref(base);
      
    return FUNC(pmath_ref(PMATH_SYMBOL_TIMES), pmath_ref(base));
  }
  
  if(pmath_is_expr_of(base, PMATH_SYMBOL_TIMES)) {
    pmath_t result = pmath_ref(base);
    size_t i;
    
    for(i = 1; i <= pmath_expr_length(base); ++i) {
      pmath_t factor = pmath_expr_get_item(base, i);
      factor = POW(factor, pmath_ref(exponent));
      result = pmath_expr_set_item(result, i, factor);
    }
    
    return result;
  }
  
  return FUNC(pmath_ref(PMATH_SYMBOL_TIMES),
              POW(pmath_ref(base), pmath_ref(exponent)));
}

static pmath_bool_t try_multiply_common_powers(pmath_t *a, pmath_t *b) {
  pmath_t baseA;
  pmath_t baseB;
  pmath_number_t numPowerA;
  pmath_number_t numPowerB;
  pmath_t restPowerA;
  pmath_t restPowerB;
  
  // x^a y^a = (x y)^a, if x, y are real
  if( pmath_is_expr_of_len(*a, PMATH_SYMBOL_POWER, 2) &&
      pmath_is_expr_of_len(*b, PMATH_SYMBOL_POWER, 2))
  {
    numPowerA = pmath_expr_get_item(*a, 2);
    numPowerB = pmath_expr_get_item(*b, 2);
    
    if(pmath_equals(numPowerA, numPowerB)) {
      baseA = pmath_expr_get_item(*a, 1);
      baseB = pmath_expr_get_item(*b, 1);
      
      if( (_pmath_number_class(baseA) & PMATH_CLASS_REAL) &&
          (_pmath_number_class(baseB) & PMATH_CLASS_REAL))
      {
        pmath_unref(*b); *b = PMATH_UNDEFINED;
        pmath_unref(numPowerA);
        pmath_unref(numPowerB);
        *a = pmath_expr_set_item(*a, 1, TIMES(baseA, baseB));
        return TRUE;
      }
      
      pmath_unref(baseA);
      pmath_unref(baseB);
    }
    
    pmath_unref(numPowerA);
    pmath_unref(numPowerB);
  }
  
  split_factor(*a, &baseA, &numPowerA, &restPowerA);
  split_factor(*b, &baseB, &numPowerB, &restPowerB);
  
  if(pmath_equals(restPowerA, restPowerB)) {
    if( pmath_same(restPowerA, PMATH_UNDEFINED) &&
        pmath_is_integer(numPowerA) &&
        pmath_is_integer(numPowerB) &&
        (!pmath_same(numPowerA, INT(1)) || !pmath_same(numPowerB, INT(1))))
    {
      pmath_bool_t a_is_product = pmath_is_expr_of(baseA, PMATH_SYMBOL_TIMES);
      pmath_bool_t b_is_product = pmath_is_expr_of(baseB, PMATH_SYMBOL_TIMES);
      
      if(a_is_product || b_is_product) {
        pmath_t expanded_a = expand_product_power(baseA, numPowerA);
        pmath_t expanded_b = expand_product_power(baseB, numPowerB);
        size_t ia, ib;
        pmath_bool_t any_simplification = FALSE;
        
        for(ib = 1; ib <= pmath_expr_length(expanded_b); ++ib) {
          pmath_t fac_b = pmath_expr_extract_item(expanded_b, ib);
          
          for(ia = 1; ia <= pmath_expr_length(expanded_a); ++ia) {
            pmath_t fac_a = pmath_expr_extract_item(expanded_a, ia);
            
            pmath_bool_t success = times_2_arg(&fac_a, &fac_b);
            any_simplification = success || any_simplification;
            
            expanded_a = pmath_expr_set_item(expanded_a, ia, fac_a);
            if(success)
              break;
          }
          
          expanded_b = pmath_expr_set_item(expanded_b, ib, fac_b);
        }
        
        if(any_simplification) {
          pmath_unref(baseA);
          pmath_unref(baseB);
          pmath_unref(numPowerA);
          pmath_unref(numPowerB);
          //pmath_unref(restPowerA); // those are PMATH_UNDEFINED
          //pmath_unref(restPowerB); // those are PMATH_UNDEFINED
          
          *a = _pmath_expr_shrink_associative(expanded_a, PMATH_UNDEFINED);
          *b = _pmath_expr_shrink_associative(expanded_b, PMATH_UNDEFINED);
          return TRUE;
        }
        
        pmath_unref(expanded_a);
        pmath_unref(expanded_b);
      }
    }
    
    if(pmath_equals(baseA, baseB)) {
      if( !pmath_same(restPowerA, PMATH_UNDEFINED) ||
          !pmath_is_number(baseA))
      {
        pmath_unref(*a);
        pmath_unref(baseB);
        pmath_unref(restPowerB);
        pmath_unref(*b);
        *b = PMATH_UNDEFINED;
        
        if(pmath_same(restPowerA, PMATH_UNDEFINED)) {
          *a = POW(baseA, PLUS(numPowerA, numPowerB));
        }
        else {
          *a = POW(baseA, TIMES(PLUS(numPowerA, numPowerB), restPowerA));
        }
        
        return TRUE;
      }
    }
    
    if(pmath_is_rational(baseA) && pmath_is_rational(baseB)) {
      pmath_integer_t baseAnum = pmath_rational_numerator(  baseA);
      pmath_integer_t baseBnum = pmath_rational_numerator(  baseB);
      pmath_integer_t baseAden = pmath_rational_denominator(baseA);
      pmath_integer_t baseBden = pmath_rational_denominator(baseB);
      
      { // (av / bw)^x (cv / dw)^y = (v/w)^(x+y) (a/b)^x (c/d)^y
        pmath_integer_t commonNum = _pmath_factor_gcd_int(&baseAnum, &baseBnum);
        pmath_integer_t commonDen = _pmath_factor_gcd_int(&baseAden, &baseBden);
        
        if( !pmath_same(commonNum, INT(1)) ||
            !pmath_same(commonDen, INT(1)))
        {
          pmath_t         facA, facB;
          pmath_integer_t commonExponent, expA, expB;
          int             sign;
          
          pmath_unref(baseA);
          pmath_unref(baseB);
          
          commonExponent = _add_nn(pmath_ref(numPowerA), pmath_ref(numPowerB));
          
          // prevent infinite recursion in Sqrt(8) -> 2 Sqrt(2) -> Sqrt(8) ...
          sign = pmath_number_sign(commonExponent);
          if( sign != 0 &&
              sign * _pmath_numbers_compare(commonExponent, numPowerA) >= 0 &&
              sign * _pmath_numbers_compare(commonExponent, numPowerB) >= 0)
          {
            pmath_unref(commonExponent);
            pmath_unref(commonNum);
            pmath_unref(commonDen);
            pmath_unref(baseAnum);
            pmath_unref(baseBnum);
            pmath_unref(baseAden);
            pmath_unref(baseBden);
            
            pmath_unref(numPowerA);
            pmath_unref(numPowerB);
            pmath_unref(restPowerA);
            pmath_unref(restPowerB);
            return FALSE;
          }
          
          pmath_unref(*a);
          pmath_unref(*b);
          
          expA = numPowerA;
          expB = numPowerB;
          if(!pmath_same(restPowerA, PMATH_UNDEFINED)) {
            commonExponent = TIMES(commonExponent, pmath_ref(restPowerA));
            expA           = TIMES(expA,           restPowerA);
            expB           = TIMES(expB,           restPowerB);
          }
          
          *a = POW(pmath_rational_new(commonNum, commonDen), commonExponent);
          
          if(pmath_same(baseAnum, INT(1)) && pmath_same(baseAden, INT(1))) {
            facA = INT(1);
            pmath_unref(expA);
          }
          else
            facA = POW(pmath_rational_new(baseAnum, baseAden), expA);
            
          if(pmath_same(baseBnum, INT(1)) && pmath_same(baseBden, INT(1))) {
            facB = INT(1);
            pmath_unref(expB);
          }
          else
            facB = POW(pmath_rational_new(baseBnum, baseBden), expB);
            
            
          if(pmath_same(facA, INT(1))) {
            if(pmath_same(facB, INT(1)))
              *b = PMATH_UNDEFINED;
            else
              *b = facB;
          }
          else if(pmath_same(facB, INT(1)))
            *b = facA;
          else
            *b = TIMES(facA, facB);
            
          return TRUE;
        }
      }
      
      { // (av / bw)^x (cw / dv)^y = (v/w)^(x-y) (a/b)^x (c/d)^y
        pmath_integer_t commonAnumBden = _pmath_factor_gcd_int(&baseAnum, &baseBden);
        pmath_integer_t commonBnumAden = _pmath_factor_gcd_int(&baseAden, &baseBnum);
        
        if( !pmath_same(commonAnumBden, INT(1)) ||
            !pmath_same(commonBnumAden, INT(1)))
        {
          pmath_t         facA, facB;
          pmath_integer_t commonExponent, expA, expB;
          int             sign;
          
          pmath_unref(baseA);
          pmath_unref(baseB);
          
          commonExponent = _add_nn(
                             pmath_ref(numPowerA),
                             pmath_number_neg(pmath_ref(numPowerB)));
                             
          // prevent infinite recursion in 1/2 * 2^(-1/2) -> (1/2)^(3/2) -> 1/2 * 2^(-1/2) ...
          sign = pmath_number_sign(commonExponent);
          if( sign != 0 &&
              sign * _pmath_numbers_compare(commonExponent, numPowerA) >= 0 &&
              sign * _pmath_numbers_compare(commonExponent, numPowerB) >= 0)
          {
            pmath_unref(commonExponent);
            pmath_unref(commonAnumBden);
            pmath_unref(commonBnumAden);
            pmath_unref(baseAnum);
            pmath_unref(baseBnum);
            pmath_unref(baseAden);
            pmath_unref(baseBden);
            
            pmath_unref(numPowerA);
            pmath_unref(numPowerB);
            pmath_unref(restPowerA);
            pmath_unref(restPowerB);
            return FALSE;
          }
          
          pmath_unref(*a);
          pmath_unref(*b);
          
          expA = numPowerA;
          expB = numPowerB;
          if(!pmath_same(restPowerA, PMATH_UNDEFINED)) {
            expA = TIMES(expA, restPowerA);
            expB = TIMES(expB, restPowerB);
          }
          
          *a = POW(pmath_rational_new(commonAnumBden, commonBnumAden), commonExponent);
          
          
          if(pmath_same(baseAnum, INT(1)) && pmath_same(baseAden, INT(1))) {
            facA = INT(1);
            pmath_unref(expA);
          }
          else
            facA = POW(pmath_rational_new(baseAnum, baseAden), expA);
            
          if(pmath_same(baseBnum, INT(1)) && pmath_same(baseBden, INT(1))) {
            facB = INT(1);
            pmath_unref(expB);
          }
          else
            facB = POW(pmath_rational_new(baseBnum, baseBden), expB);
            
            
          if(pmath_same(facA, INT(1))) {
            if(pmath_same(facB, INT(1)))
              *b = PMATH_UNDEFINED;
            else
              *b = facB;
          }
          else if(pmath_same(facB, INT(1)))
            *b = facA;
          else
            *b = TIMES(facA, facB);
            
          return TRUE;
        }
      }
      
      pmath_unref(baseAnum);
      pmath_unref(baseBnum);
      pmath_unref(baseAden);
      pmath_unref(baseBden);
    }
  }
  
  pmath_unref(baseA);
  pmath_unref(baseB);
  pmath_unref(numPowerA);
  pmath_unref(numPowerB);
  pmath_unref(restPowerA);
  pmath_unref(restPowerB);
  return FALSE;
}

static pmath_bool_t try_multiply_overflow(pmath_t *a, pmath_t *b) {
  if( (pmath_equals(*a, _pmath_object_overflow) &&
       pmath_equals(*b, _pmath_object_underflow)) ||
      (pmath_equals(*a, _pmath_object_underflow) &&
       pmath_equals(*b, _pmath_object_overflow)))
  {
    pmath_unref(*a);
    pmath_unref(*b);
    *a = pmath_ref(PMATH_SYMBOL_UNDEFINED);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  if( (pmath_equals(*a, _pmath_object_overflow) ||
       pmath_equals(*a, _pmath_object_underflow)) &&
      pmath_is_numeric(*b))
  {
    if( pmath_equals(*a, _pmath_object_overflow) ||
        pmath_equals(*a, _pmath_object_underflow))
    {
      pmath_unref(*b);
      *b = PMATH_UNDEFINED;
    }
    return TRUE;
  }
  
  if( (pmath_equals(*b, _pmath_object_overflow) ||
       pmath_equals(*b, _pmath_object_underflow)) &&
      pmath_is_numeric(*a))
  {
    pmath_unref(*a);
    *a = *b;
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  return FALSE;
}

static pmath_bool_t times_2_arg(pmath_t *a, pmath_t *b) {
  if(pmath_is_number(*a)) {
    if(try_multiply_real_number_to(a, b))
      return TRUE;
  }
  else if(_pmath_is_nonreal_complex(*a)) {
    if(try_multiply_nonreal_complex_to(a, b))
      return TRUE;
  }
    
  if(pmath_is_interval(*a)) {
    if(try_multiply_interval_to(a, b))
      return TRUE;
  }
  
  if(try_multiply_infinities(a, b))
    return TRUE;
    
  if(try_multiply_common_powers(a, b))
    return TRUE;
    
  return try_multiply_overflow(a, b);
}

PMATH_PRIVATE pmath_t builtin_times(pmath_expr_t expr) {
  size_t ia, ib;
  const size_t elen = pmath_expr_length(expr);
//  _pmath_timer_t last_change;
//  pmath_bool_t any_change;

  if(elen == 0) {
    pmath_unref(expr);
    return PMATH_FROM_INT32(1);
  }
  
  if(elen == 1) {
    pmath_t item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return item;
  }
  
//  /* Using
//     a = pmath_expr_extract_item(expr, ia);
//     expr = pmath_expr_set_item(expr, ia, a);
//     might mark the expression as changed because ..._extract_... temporaryliy
//     writes PMATH_UNDEFINED which is later compared with a in ..._set_...
//     and since both do not equal, the change timer is set.
//
//     So we have to reset that timer ourself when nothing realy happend.
//
//     An alternative would be to implement whe loop below twice: once with
//     direct access to the items for pmath_refcount(expr) == 1 && expr is a
//     PMATH_TYPE_EXPRESSION_GENERAL and once with pmath_expr_get_item() for all
//     other cases.
//  */
//
//  last_change = ((struct _pmath_timed_t*)PMATH_AS_PTR(expr))->last_change;
//  any_change = FALSE;

  ia = 1;
  while(ia < elen) {
    pmath_t a = pmath_expr_get_item(expr, ia); // = pmath_expr_extract_item(expr, ia);
    
    if(!pmath_same(a, PMATH_UNDEFINED)) {
      ib = ia + 1;
      while(ib <= elen) {
        pmath_t b = pmath_expr_get_item(expr, ib); // = pmath_expr_extract_item(expr, ib);
        
        if(!pmath_same(b, PMATH_UNDEFINED)) {
          times_2_arg(&a, &b);
//          if(times_2_arg(&a, &b))
//            any_change = TRUE;

          expr = pmath_expr_set_item(expr, ib, b);
        }
        ++ib;
      }
      
      expr = pmath_expr_set_item(expr, ia, a);
    }
    ++ia;
  }
  
//  if(!any_change){
//    ((struct _pmath_timed_t*)PMATH_AS_PTR(expr))->last_change = last_change;
//    return expr;
//  }

  return _pmath_expr_shrink_associative(expr, PMATH_UNDEFINED);
}
