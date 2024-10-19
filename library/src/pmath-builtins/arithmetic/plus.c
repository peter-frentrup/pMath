#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>


extern pmath_symbol_t pmath_System_Complex;
extern pmath_symbol_t pmath_System_Infinity;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_Times;
extern pmath_symbol_t pmath_System_Undefined;

static pmath_integer_t _addmul_iii( // intA + intB * intC
  pmath_integer_t intA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB, // will be freed. not PMATH_NULL!
  pmath_integer_t intC  // will be freed. not PMATH_NULL!
) {
  pmath_mpint_t result;
  
  assert(pmath_is_integer(intA));
  assert(pmath_is_integer(intB));
  assert(pmath_is_integer(intC));
  
  if(pmath_is_int32(intA)) {
    result = _pmath_create_mp_int(PMATH_AS_INT32(intA));
  }
  else {
    result = _pmath_create_mp_int(0);
    
    if(!pmath_is_null(result))
      mpz_set(PMATH_AS_MPZ(result), PMATH_AS_MPZ(intA));
      
    pmath_unref(intA);
  }
  
  if(pmath_is_null(result)) {
    pmath_unref(intB);
    pmath_unref(intC);
    return PMATH_NULL;
  }
  
  if(pmath_is_int32(intB)) {
    int b = PMATH_AS_INT32(intB);
    
    if(pmath_is_int32(intC)) {
      intC = _pmath_create_mp_int(PMATH_AS_INT32(intC));
      
      if(pmath_is_null(intC)) {
        pmath_unref(intB);
        pmath_unref(result);
        return PMATH_NULL;
      }
    }
    
    assert(pmath_is_mpint(intC));
    
    if(b < 0) {
      mpz_submul_ui(
        PMATH_AS_MPZ(result),
        PMATH_AS_MPZ(intC),
        (unsigned) - b);
    }
    else {
      mpz_addmul_ui(
        PMATH_AS_MPZ(result),
        PMATH_AS_MPZ(intC),
        (unsigned)b);
    }
    
    pmath_unref(intC);
    return _pmath_mp_int_normalize(result);
  }
  
  assert(pmath_is_mpint(intB));
  
  if(pmath_is_int32(intC)) {
    int c = PMATH_AS_INT32(intC);
    
    if(c < 0) {
      mpz_submul_ui(
        PMATH_AS_MPZ(result),
        PMATH_AS_MPZ(intB),
        (unsigned) - c);
    }
    else {
      mpz_addmul_ui(
        PMATH_AS_MPZ(result),
        PMATH_AS_MPZ(intB),
        (unsigned)c);
    }
    
    pmath_unref(intB);
    return _pmath_mp_int_normalize(result);
  }
  
  mpz_addmul(PMATH_AS_MPZ(result), PMATH_AS_MPZ(intB), PMATH_AS_MPZ(intC));
  pmath_unref(intB);
  pmath_unref(intC);
  return result;
}

static pmath_integer_t _add_ii(
  pmath_integer_t intA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB  // will be freed. not PMATH_NULL!
) {
  pmath_integer_t result;
  
  assert(pmath_is_integer(intA));
  assert(pmath_is_integer(intB));
  
  if(pmath_is_int32(intA)) {
    int a = PMATH_AS_INT32(intA);
    
    if(pmath_is_int32(intB)) {
      int b = PMATH_AS_INT32(intB);
      
      int64_t sum64 = (int64_t)a + b;
      if(sum64 == (int32_t)sum64)
        return PMATH_FROM_INT32((int32_t)sum64);
        
      intB = _pmath_create_mp_int(b);
      if(pmath_is_null(intB))
        return PMATH_NULL;
    }
    
    assert(pmath_is_mpint(intB));
    
    if(pmath_refcount(intB) == 1) {
      result = pmath_ref(intB);
    }
    else {
      result = _pmath_create_mp_int(0);
      
      if(pmath_is_null(result)) {
        pmath_unref(intB);
        return result;
      }
    }
    
    if(a < 0) {
      mpz_sub_ui(
        PMATH_AS_MPZ(result),
        PMATH_AS_MPZ(intB),
        (unsigned) - a);
    }
    else {
      mpz_add_ui(
        PMATH_AS_MPZ(result),
        PMATH_AS_MPZ(intB),
        (unsigned)a);
    }
    
    pmath_unref(intB);
    return _pmath_mp_int_normalize(result);
  }
  
  assert(pmath_is_mpint(intA));
  
  if(pmath_is_int32(intB)) {
    int b = PMATH_AS_INT32(intB);
    
    if(pmath_refcount(intA) == 1) {
      result = pmath_ref(intA);
    }
    else {
      result = _pmath_create_mp_int(0);
      
      if(pmath_is_null(result)) {
        pmath_unref(intA);
        return result;
      }
    }
    
    if(b < 0) {
      mpz_sub_ui(
        PMATH_AS_MPZ(result),
        PMATH_AS_MPZ(intA),
        (unsigned) - b);
    }
    else {
      mpz_add_ui(
        PMATH_AS_MPZ(result),
        PMATH_AS_MPZ(intA),
        (unsigned)b);
    }
    
    pmath_unref(intA);
    return _pmath_mp_int_normalize(result);
  }
  
  assert(pmath_is_mpint(intB));
  
  if(pmath_refcount(intA) == 1) {
    result = pmath_ref(intA);
  }
  else if(pmath_refcount(intB) == 1) {
    result = pmath_ref(intB);
  }
  else {
    result = _pmath_create_mp_int(0);
    
    if(pmath_is_null(result)) {
      pmath_unref(intA);
      pmath_unref(intB);
      return result;
    }
  }
  
  mpz_add(
    PMATH_AS_MPZ(result),
    PMATH_AS_MPZ(intA),
    PMATH_AS_MPZ(intB));
    
  pmath_unref(intA);
  pmath_unref(intB);
  return _pmath_mp_int_normalize(result);
}

static pmath_integer_t _add_ss(
  pmath_integer_t intA, // must yield pmath_is_int32
  pmath_integer_t intB  // must yield pmath_is_int32
) {
  int64_t sum64;
  int a, b;
  
  assert(pmath_is_int32(intA));
  assert(pmath_is_int32(intB));
  
  a = PMATH_AS_INT32(intA);
  b = PMATH_AS_INT32(intB);
  
  sum64 = (int64_t)a + b;
  if(sum64 == (int32_t)sum64)
    return PMATH_FROM_INT32((int32_t)sum64);
    
  intB = _pmath_create_mp_int(b);
  if(pmath_is_null(intB))
    return PMATH_NULL;
    
  return _add_ii(intA, intB);
}

static pmath_rational_t _add_qi(
  pmath_quotient_t quotA, // will be freed. not PMATH_NULL!
  pmath_integer_t  intB   // will be freed. not PMATH_NULL!
) { // u/v + w = (u+v*w)/v
  pmath_integer_t numerator   = pmath_ref(PMATH_QUOT_NUM(quotA));
  pmath_integer_t denominator = pmath_ref(PMATH_QUOT_DEN(quotA));
  pmath_unref(quotA);
  
  denominator = pmath_ref(denominator);
  // ... because next frees denominator twice
  return pmath_rational_new(
           _addmul_iii(
             numerator,
             denominator,
             intB),
           denominator);
}

static pmath_rational_t _add_qq(
  pmath_quotient_t quotA, // will be freed. not PMATH_NULL!
  pmath_integer_t  quotB  // will be freed. not PMATH_NULL!
) { // u/v + w/x = (u*x+v*w)/(v*x)
  pmath_integer_t numeratorA   = pmath_ref(PMATH_QUOT_NUM(quotA));
  pmath_integer_t denominatorA = pmath_ref(PMATH_QUOT_DEN(quotA));
  
  pmath_integer_t numeratorB   = pmath_ref(PMATH_QUOT_NUM(quotB));
  pmath_integer_t denominatorB = pmath_ref(PMATH_QUOT_DEN(quotB));
  pmath_unref(quotA);
  pmath_unref(quotB);
  
  numeratorA = _mul_ii( // u*x
                 numeratorA,
                 pmath_ref(denominatorB));
                 
  if(pmath_is_null(numeratorA)) {
    pmath_unref(numeratorB);
    pmath_unref(denominatorB);
    pmath_unref(denominatorA);
    return PMATH_NULL;
  }
  
  denominatorA = pmath_ref(denominatorA);
  // ... because next frees denominatorA twice
  return pmath_rational_new(
           _addmul_iii(
             numeratorA,
             denominatorA,
             numeratorB),
           _mul_ii(
             denominatorA,
             denominatorB));
}

static pmath_t _add_fi(
  pmath_mpfloat_t floatA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB    // will be freed. not PMATH_NULL!
) {
  pmath_mpfloat_t result;
  
  assert(pmath_is_mpfloat(floatA));
  assert(pmath_is_integer(intB));
  
  result = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(floatA));
  
  if(pmath_is_null(result)) {
    pmath_unref(floatA);
    pmath_unref(intB);
    return PMATH_NULL;
  }
  
  if(pmath_is_int32(intB)) {
    arb_add_si(PMATH_AS_ARB(result), PMATH_AS_ARB(floatA), PMATH_AS_INT32(intB), PMATH_AS_ARB_WORKING_PREC(result));
  }
  else {
    fmpz_t tmpB;
    assert(pmath_is_mpint(intB));
    
    fmpz_init(tmpB);
    fmpz_set_mpz(tmpB, PMATH_AS_MPZ(intB));
    
    arb_add_fmpz(PMATH_AS_ARB(result), PMATH_AS_ARB(floatA), tmpB, PMATH_AS_ARB_WORKING_PREC(result));
    
    fmpz_clear(tmpB);
    pmath_unref(intB);
  }
  
  pmath_unref(floatA);
  return _pmath_float_exceptions(result);
}

static pmath_t _add_ff(
  pmath_mpfloat_t floatA, // will be freed. not PMATH_NULL!
  pmath_mpfloat_t floatB  // will be freed. not PMATH_NULL!
) {
  pmath_mpfloat_t result;
  slong prec;
  
  assert(pmath_is_mpfloat(floatA));
  assert(pmath_is_mpfloat(floatB));
  
  prec = FLINT_MAX(PMATH_AS_ARB_WORKING_PREC(floatA), PMATH_AS_ARB_WORKING_PREC(floatB));
  result = _pmath_create_mp_float(prec);
  if(pmath_is_null(result)) {
    pmath_unref(floatA);
    pmath_unref(floatB);
    return PMATH_NULL;
  }
  
  arb_add(PMATH_AS_ARB(result), PMATH_AS_ARB(floatA), PMATH_AS_ARB(floatB), prec);
    
  pmath_unref(floatA);
  pmath_unref(floatB);
  return _pmath_float_exceptions(result);
}

static pmath_t _add_fq(
  pmath_mpfloat_t  floatA, // will be freed. not PMATH_NULL!
  pmath_quotient_t quotB   // will be freed. not PMATH_NULL!
) {
  pmath_mpfloat_t floatB;
  
  assert(pmath_is_mpfloat(floatA));
  assert(pmath_is_quotient(quotB));
  
  floatB = _pmath_create_mp_float_from_q(quotB, PMATH_AS_ARB_WORKING_PREC(floatA));
  if(pmath_is_null(floatB)) {
    pmath_unref(floatA);
    return PMATH_NULL;
  }
  
  return _add_ff(floatA, floatB);
}

static pmath_t _add_mi(
  pmath_float_t   floatA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB    // will be freed. not PMATH_NULL!
) {
  double d;
  
  assert(pmath_is_double(floatA));
  assert(pmath_is_integer(intB));
  
  d = PMATH_AS_DOUBLE(floatA);
  
  if(pmath_is_int32(intB)) {
    d += PMATH_AS_INT32(intB);
  }
  else {
    assert(pmath_is_mpint(intB));
    
    d += mpz_get_d(PMATH_AS_MPZ(intB));
  }
  
  if(!isfinite(d))
    return _add_nn(_pmath_convert_to_mp_float(floatA), intB);
    
  pmath_unref(intB);
  pmath_unref(floatA);
  return PMATH_FROM_DOUBLE(d);
}

static pmath_t _add_mq(
  pmath_float_t    floatA, // will be freed. not PMATH_NULL!
  pmath_quotient_t quotB   // will be freed. not PMATH_NULL!
) {
  double d;
  
  assert(pmath_is_double(floatA));
  assert(pmath_is_quotient(quotB));
  
  if(pmath_is_int32(PMATH_QUOT_NUM(quotB))) {
    d = PMATH_AS_INT32(PMATH_QUOT_NUM(quotB));
  }
  else {
    assert(pmath_is_mpint(PMATH_QUOT_NUM(quotB)));
    
    d = mpz_get_d(PMATH_AS_MPZ(PMATH_QUOT_NUM(quotB)));
  }
  
  if(pmath_is_int32(PMATH_QUOT_DEN(quotB))) {
    d /= PMATH_AS_INT32(PMATH_QUOT_DEN(quotB));
  }
  else {
    assert(pmath_is_mpint(PMATH_QUOT_DEN(quotB)));
    
    d /= mpz_get_d(PMATH_AS_MPZ(PMATH_QUOT_DEN(quotB)));
  }
  
  d += PMATH_AS_DOUBLE(floatA);
  
  if(!isfinite(d))
    return _add_nn(_pmath_convert_to_mp_float(floatA), quotB);
    
  pmath_unref(quotB);
  pmath_unref(floatA);
  return PMATH_FROM_DOUBLE(d);
}

static pmath_t _add_mf(
  pmath_float_t    floatA, // will be freed. not PMATH_NULL!
  pmath_mpfloat_t  floatB  // will be freed. not PMATH_NULL!
) {
  double d;
  
  assert(pmath_is_double(floatA));
  assert(pmath_is_mpfloat(floatB));
  
  d = PMATH_AS_DOUBLE(floatA) + arf_get_d(arb_midref(PMATH_AS_ARB(floatB)), ARF_RND_NEAR);
  
  if(!isfinite(d))
    return _add_nn(_pmath_convert_to_mp_float(floatA), floatB);
    
  pmath_unref(floatB);
  pmath_unref(floatA);
  return PMATH_FROM_DOUBLE(d);
}

static pmath_t _add_mm(
  pmath_float_t  floatA, // will be freed. not PMATH_NULL!
  pmath_float_t  floatB  // will be freed. not PMATH_NULL!
) {
  double d;
  
  assert(pmath_is_double(floatA));
  assert(pmath_is_double(floatB));
  
  d = PMATH_AS_DOUBLE(floatA) + PMATH_AS_DOUBLE(floatB);
  
  if(!isfinite(d)) {
    return _add_nn(
             _pmath_convert_to_mp_float(floatA),
             _pmath_convert_to_mp_float(floatB));
  }
  
  pmath_unref(floatA);
  pmath_unref(floatB);
  return PMATH_FROM_DOUBLE(d);
}

PMATH_PRIVATE pmath_t _add_nn(
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
      return _add_mm(numA, numB);
      
    if(pmath_is_int32(numB))
      return _add_mi(numA, numB);
      
    assert(pmath_is_pointer(numB));
    
    switch(PMATH_AS_PTR(numB)->type_shift) {
      case PMATH_TYPE_SHIFT_MP_INT:         return _add_mi(numA, numB);
      case PMATH_TYPE_SHIFT_QUOTIENT:       return _add_mq(numA, numB);
      case PMATH_TYPE_SHIFT_MP_FLOAT:       return _add_mf(numA, numB);
    }
    
    assert("invalid number type" && 0);
    return PMATH_NULL;
  }
  
  if(pmath_is_double(numB))
    return _add_nn(numB, numA);
    
  if(pmath_is_int32(numA)) {
    if(pmath_is_int32(numB))
      return _add_ss(numB, numA);
      
    assert(pmath_is_pointer(numB));
    
    switch(PMATH_AS_PTR(numB)->type_shift) {
      case PMATH_TYPE_SHIFT_MP_INT:         return _add_ii(numB, numA);
      case PMATH_TYPE_SHIFT_QUOTIENT:       return _add_qi(numB, numA);
      case PMATH_TYPE_SHIFT_MP_FLOAT:       return _add_fi(numB, numA);
    }
    
    assert("invalid number type" && 0);
    return PMATH_NULL;
  }
  
  if(pmath_is_int32(numB))
    return _add_nn(numB, numA);
    
  assert(pmath_is_pointer(numA));
  assert(pmath_is_pointer(numB));
  
  switch(PMATH_AS_PTR(numA)->type_shift) {
    case PMATH_TYPE_SHIFT_MP_INT: {
        switch(PMATH_AS_PTR(numB)->type_shift) {
          case PMATH_TYPE_SHIFT_MP_INT:         return _add_ii(numA, numB);
          case PMATH_TYPE_SHIFT_QUOTIENT:       return _add_qi(numB, numA);
          case PMATH_TYPE_SHIFT_MP_FLOAT:       return _add_fi(numB, numA);
        }
      } break;
      
    case PMATH_TYPE_SHIFT_QUOTIENT: {
        switch(PMATH_AS_PTR(numB)->type_shift) {
          case PMATH_TYPE_SHIFT_MP_INT:         return _add_qi(numA, numB);
          case PMATH_TYPE_SHIFT_QUOTIENT:       return _add_qq(numA, numB);
          case PMATH_TYPE_SHIFT_MP_FLOAT:       return _add_fq(numB, numA);
        }
      } break;
      
    case PMATH_TYPE_SHIFT_MP_FLOAT: {
        switch(PMATH_AS_PTR(numB)->type_shift) {
          case PMATH_TYPE_SHIFT_MP_INT:         return _add_fi(numA, numB);
          case PMATH_TYPE_SHIFT_QUOTIENT:       return _add_fq(numA, numB);
          case PMATH_TYPE_SHIFT_MP_FLOAT:       return _add_ff(numA, numB);
        }
      } break;
  }
  
  assert("invalid number type" && 0);
  
  return PMATH_NULL;
}

PMATH_PRIVATE void _pmath_split_summand(
  pmath_t  summand,         // wont be freed
  pmath_t *out_num_factor,
  pmath_t *out_rest
) {
  if(pmath_is_expr(summand)) {
    pmath_t head = pmath_expr_get_item(summand, 0);
    pmath_unref(head);
    
    if(pmath_same(head, pmath_System_Times)) {
      size_t len = pmath_expr_length(summand);
      
      if(len > 1) {
        pmath_t first = pmath_expr_get_item(summand, 1);
        if( pmath_is_number(first) || _pmath_is_nonreal_complex_number(first)) {
          *out_num_factor = first;
          
          if(len == 2) {
            *out_rest = pmath_expr_get_item(summand, 2);
            return;
          }
          
          *out_rest = pmath_expr_get_item_range(summand, 2, len - 1);
          return;
        }
        pmath_unref(first);
      }
      
      *out_num_factor = PMATH_FROM_INT32(1);
      *out_rest = pmath_ref(summand);
      return;
    }
    
    if(_pmath_is_nonreal_complex_number(summand)) {
      *out_num_factor = pmath_ref(summand);
      *out_rest = PMATH_UNDEFINED;
      return;
    }
  }
  
  if(pmath_is_number(summand)) {
    *out_num_factor = pmath_ref(summand);
    *out_rest = PMATH_UNDEFINED;
    return;
  }
  
  *out_num_factor = PMATH_FROM_INT32(1);
  *out_rest = pmath_ref(summand);
}

static pmath_bool_t try_add_nonreal_complex_to_noncomplex(pmath_t *a, pmath_t *b);

static pmath_bool_t try_add_real_number_to(pmath_number_t *a, pmath_t *b) {
  assert(pmath_is_number(*a));
  
  if(pmath_is_number(*b)) {
    *a = _add_nn(*a, *b);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  if(pmath_equals(*a, PMATH_FROM_INT32(0))) {
    pmath_unref(*a);
    *a = *b;
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
  
  if(_pmath_is_nonreal_complex_number(*b)) 
    return try_add_nonreal_complex_to_noncomplex(b, a);
  
  if(pmath_is_float(*a) && pmath_is_numeric(*b)) {
    *b = pmath_set_precision(*b, pmath_precision(pmath_ref(*a)));
    return TRUE;
  }
  
  return FALSE;
}

static pmath_bool_t try_add_nonreal_complex_to_noncomplex(pmath_t *a, pmath_t *b) {
  pmath_t re, im;
  
  assert(_pmath_is_nonreal_complex_number(*a));
  
  re = pmath_expr_get_item(*a, 1);
  im = pmath_expr_get_item(*a, 2);
  
  if(pmath_is_number(*b)) {
    pmath_unref(im);
    re = _add_nn(re, *b);
    *b = PMATH_UNDEFINED;
    *a = pmath_expr_set_item(*a, 1, re);
    return TRUE;
  }
  
  if(pmath_is_float(re) || pmath_is_float(im)) {
    *b = pmath_set_precision(*b, pmath_precision(pmath_ref(*a)));
    pmath_unref(re);
    pmath_unref(im);
    return TRUE;
  }
  
  pmath_unref(re);
  pmath_unref(im);
  return FALSE;
}

static pmath_bool_t try_add_nonreal_complex_to(pmath_t *a, pmath_t *b) {
  assert(_pmath_is_nonreal_complex_number(*a));
  
  if(_pmath_is_nonreal_complex_number(*b)) {
    pmath_t reim_a[2];
    pmath_t reim_b[2];
    int i;
      
    reim_a[0] = pmath_expr_get_item(*a, 1);
    reim_a[1] = pmath_expr_get_item(*a, 2);
    
    reim_b[0] = pmath_expr_get_item(*b, 1);
    reim_b[1] = pmath_expr_get_item(*b, 2);
    
    for(i = 0; i < 2; ++i) {
      reim_a[i] = _add_nn(reim_a[i], reim_b[i]);
      reim_b[i] = PMATH_UNDEFINED;
    }
    
    if(pmath_same(reim_b[0], PMATH_UNDEFINED) && pmath_same(reim_b[1] , PMATH_UNDEFINED)) {
      pmath_unref(*b);
      *b = PMATH_UNDEFINED;
    }
    else if(pmath_same(reim_b[0], PMATH_UNDEFINED)) {
      pmath_unref(*b);
      *b = COMPLEX(INT(0), reim_b[1]);
    }
    else if(pmath_same(reim_b[1], PMATH_UNDEFINED)) {
      pmath_unref(*b);
      *b = reim_b[1];
    }
    else {
      pmath_unref(*b);
      *b = COMPLEX(reim_b[0], reim_b[1]);
    }
    
    *a = pmath_expr_set_item(*a, 1, reim_a[0]);
    *a = pmath_expr_set_item(*a, 2, reim_a[1]);
    return TRUE;
  }
  
  return try_add_nonreal_complex_to_noncomplex(a, b);
}

static pmath_bool_t try_add_overflow(pmath_t *a, pmath_t *b) {
  if( pmath_equals(*a, _pmath_object_overflow) &&
      (pmath_equals(*b, _pmath_object_overflow)  ||
       pmath_equals(*b, _pmath_object_underflow) ||
       pmath_is_numeric(*b)))
  {
    pmath_unref(*b);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  if(pmath_equals(*b, _pmath_object_overflow) && pmath_is_numeric(*a)) {
    pmath_unref(*a);
    *a = *b;
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  if(pmath_equals(*a, _pmath_object_underflow) && pmath_is_numeric(*b)) {
    pmath_unref(*b);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  return FALSE;
}

static pmath_bool_t try_add_infinities(pmath_t *a, pmath_t *b) {
  pmath_t ainfdir = _pmath_directed_infinity_direction(*a);
  pmath_t binfdir = _pmath_directed_infinity_direction(*b);
  
  if(!pmath_is_null(ainfdir) && !pmath_is_null(binfdir)) {
    pmath_t sum;
    
    if(pmath_equals(ainfdir, binfdir)) {
      if(pmath_equals(ainfdir, PMATH_FROM_INT32(0))) {
        pmath_unref(ainfdir);
        pmath_unref(binfdir);
        pmath_unref(*a);
        pmath_unref(*b);
        *a = pmath_ref(pmath_System_Undefined);
        *b = PMATH_UNDEFINED;
        return TRUE;
      }
      pmath_unref(ainfdir);
      pmath_unref(binfdir);
      pmath_unref(*b);
      *b = PMATH_UNDEFINED;
      return TRUE;
    }
    
    sum = pmath_evaluate(
            pmath_expr_new_extended(
              pmath_ref(pmath_System_Plus), 2,
              ainfdir,
              binfdir));
              
    if(pmath_equals(sum, PMATH_FROM_INT32(0))) {
      pmath_unref(sum);
      pmath_message(pmath_System_Infinity, "indet", 1,
                    pmath_expr_new_extended(
                      pmath_ref(pmath_System_Plus), 2,
                      *a,
                      *b));
      *a = pmath_ref(pmath_System_Undefined);
      *b = PMATH_UNDEFINED;
    }
    return TRUE;
  }
  
  if(!pmath_is_null(ainfdir)) {
    pmath_unref(ainfdir);
    pmath_unref(*b);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  if(!pmath_is_null(binfdir)) {
    pmath_unref(binfdir);
    pmath_unref(*a);
    *a = *b;
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  return FALSE;
}

static pmath_bool_t try_add_common_factors(pmath_t *a, pmath_t *b) {
  pmath_number_t numFactorA;
  pmath_number_t numFactorB;
  pmath_t restA;
  pmath_t restB;
  _pmath_split_summand(*a, &numFactorA, &restA);
  _pmath_split_summand(*b, &numFactorB, &restB);
  
  if(pmath_equals(restA, restB)) {
    pmath_unref(*a);
    pmath_unref(*b);
    pmath_unref(restB);
    *a = pmath_expr_new_extended(
           pmath_ref(pmath_System_Times), 2,
           pmath_expr_new_extended(
             pmath_ref(pmath_System_Plus), 2,
             numFactorA,
             numFactorB),
           restA);
    *b = PMATH_UNDEFINED;
    return TRUE;
  }
  
  pmath_unref(numFactorA);
  pmath_unref(numFactorB);
  pmath_unref(restA);
  pmath_unref(restB);
  
  return FALSE;
}

static void plus_2_arg(pmath_t *a, pmath_t *b) {
  if(pmath_is_number(*a)) {
    if(try_add_real_number_to(a, b))
      return;
  }
  else if(_pmath_is_nonreal_complex_number(*a)) {
    if(try_add_nonreal_complex_to(a, b))
      return;
  }
  else if(_pmath_is_nonreal_complex_number(*b)) {
    if(try_add_nonreal_complex_to_noncomplex(b, a))
      return;
  }
  
  if(try_add_overflow(a, b))
    return;
    
  if(try_add_infinities(a, b))
    return;
    
  if(try_add_common_factors(a, b))
    return;
}

PMATH_PRIVATE pmath_t builtin_plus(pmath_expr_t expr) {
  size_t ia, ib;
  const size_t elen = pmath_expr_length(expr);
  
  if(elen == 0) {
    pmath_unref(expr);
    return PMATH_FROM_INT32(0);
  }
  
  if(elen == 1) {
    pmath_t item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return item;
  }
  
  ia = 1;
  while(ia < elen) {
    pmath_t a = pmath_expr_get_item(expr, ia);
    if(!pmath_same(a, PMATH_UNDEFINED)) {
      ib = ia + 1;
      while(ib <= elen) {
        pmath_t b = pmath_expr_get_item(expr, ib);
        if(!pmath_same(b, PMATH_UNDEFINED)) {
          plus_2_arg(&a, &b);
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
