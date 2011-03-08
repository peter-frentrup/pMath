#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/number-theory-private.h>

static pmath_integer_t _addmul_iii( // intA + intB * intC
  pmath_integer_t intA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB, // will be freed. not PMATH_NULL!
  pmath_integer_t intC  // will be freed. not PMATH_NULL!
){
  pmath_integer_t result;
  
  assert(pmath_is_integer(intA));
  assert(pmath_is_integer(intB));
  assert(pmath_is_integer(intC));
  
  if(PMATH_AS_PTR(intA)->refcount == 1){
    result = intA;
  }
  else{
    result = _pmath_create_integer();
    if(pmath_is_null(result)){
      pmath_unref(intA);
      pmath_unref(intB);
      pmath_unref(intC);
      return PMATH_NULL;
    }
    mpz_set(PMATH_AS_MPZ(result), PMATH_AS_MPZ(intA));
    pmath_unref(intA);
  }

  if(!pmath_is_null(result))
    mpz_addmul(PMATH_AS_MPZ(result), PMATH_AS_MPZ(intB), PMATH_AS_MPZ(intC));
  pmath_unref(intB);
  pmath_unref(intC);
  return result;
}

static pmath_integer_t _add_ii(
  pmath_integer_t intA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB  // will be freed. not PMATH_NULL!
){
  pmath_integer_t result;
  
  assert(pmath_is_integer(intA));
  assert(pmath_is_integer(intB));
  
  if(PMATH_AS_PTR(intA)->refcount == 1){
    result = pmath_ref(intA);
  }
  else if(PMATH_AS_PTR(intB)->refcount == 1){
    result = pmath_ref(intB);
  }
  else{
    result = _pmath_create_integer();
    if(pmath_is_null(result)){
      pmath_unref(intA);
      pmath_unref(intB);
      return PMATH_NULL;
    }
  }
  
  mpz_add(
    PMATH_AS_MPZ(result),
    PMATH_AS_MPZ(intA),
    PMATH_AS_MPZ(intB));
          
  pmath_unref(intA);
  pmath_unref(intB);
  return result;
}

static pmath_rational_t _add_qi(
  pmath_quotient_t quotA, // will be freed. not PMATH_NULL!
  pmath_integer_t  intB   // will be freed. not PMATH_NULL!
){ // u/v + w = (u+v*w)/v
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
){ // u/v + w/x = (u*x+v*w)/(v*x)
  pmath_integer_t numeratorA   = pmath_ref(PMATH_QUOT_NUM(quotA));
  pmath_integer_t denominatorA = pmath_ref(PMATH_QUOT_DEN(quotA));

  pmath_integer_t numeratorB   = pmath_ref(PMATH_QUOT_NUM(quotB));
  pmath_integer_t denominatorB = pmath_ref(PMATH_QUOT_DEN(quotB));
  pmath_unref(quotA);
  pmath_unref(quotB);

  numeratorA = _mul_ii( // u*x
    numeratorA,
    pmath_ref(denominatorB));

  if(pmath_is_null(numeratorA)){
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
  pmath_float_t   floatA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB    // will be freed. not PMATH_NULL!
){
  pmath_float_t result;
  
  assert(pmath_is_mpfloat(floatA));
  assert(pmath_is_integer(intB));
  
  result = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(floatA)));
  
  if(pmath_is_null(result)){
    pmath_unref(floatA);
    pmath_unref(intB);
    return PMATH_NULL;
  }
  
  // error does not change
  mpfr_set(
    PMATH_AS_MP_ERROR(result),
    PMATH_AS_MP_ERROR(floatA), 
    MPFR_RNDU);
    
  mpfr_add_z(
    PMATH_AS_MP_VALUE(result),
    PMATH_AS_MP_VALUE(floatA),
    PMATH_AS_MPZ(intB),
    MPFR_RNDN);
  
  _pmath_mp_float_normalize(result);
  
  pmath_unref(floatA);
  pmath_unref(intB);
  return _pmath_float_exceptions(result);
}

static pmath_t _add_fq(
  pmath_float_t    floatA, // will be freed. not PMATH_NULL!
  pmath_quotient_t quotB   // will be freed. not PMATH_NULL!
){
  pmath_float_t result;
  pmath_float_t tmp;
  mp_prec_t prec;
  
  assert(pmath_is_mpfloat(floatA));
  assert(pmath_is_quotient(quotB));
  
  prec = mpfr_get_prec(PMATH_AS_MP_VALUE(floatA));
  
  result = _pmath_create_mp_float(prec);
  tmp    = _pmath_create_mp_float(prec);
  if(pmath_is_null(result) || pmath_is_null(tmp)){
    pmath_unref(result);
    pmath_unref(tmp);
    pmath_unref(floatA);
    pmath_unref(quotB);
    return PMATH_NULL;
  }

  // error does not change
  mpfr_set(
    PMATH_AS_MP_ERROR(result), 
    PMATH_AS_MP_ERROR(floatA), 
    MPFR_RNDU);
  
  mpfr_set_z(
    PMATH_AS_MP_VALUE(result),
    PMATH_AS_MPZ(PMATH_QUOT_NUM(quotB)),
    MPFR_RNDN);

  mpfr_div_z(
    PMATH_AS_MP_VALUE(tmp),
    PMATH_AS_MP_VALUE(result),
    PMATH_AS_MPZ(PMATH_QUOT_DEN(quotB)),
    MPFR_RNDN);

  mpfr_add(
    PMATH_AS_MP_VALUE(result),
    PMATH_AS_MP_VALUE(tmp),
    PMATH_AS_MP_VALUE(floatA),
    MPFR_RNDN);
  
  _pmath_mp_float_normalize(result);

  pmath_unref(floatA);
  pmath_unref(quotB);
  pmath_unref(tmp);
  return _pmath_float_exceptions(result);
}

static pmath_t _add_ff(
  pmath_float_t floatA, // will be freed. not PMATH_NULL!
  pmath_float_t floatB  // will be freed. not PMATH_NULL!
){
  pmath_float_t result;
  mp_prec_t prec;
  
  assert(pmath_is_mpfloat(floatA));
  assert(pmath_is_mpfloat(floatB));
  
  prec =    mpfr_get_prec(PMATH_AS_MP_VALUE(floatA));
  if(prec < mpfr_get_prec(PMATH_AS_MP_VALUE(floatB)))
     prec = mpfr_get_prec(PMATH_AS_MP_VALUE(floatB));
  
  result = _pmath_create_mp_float(prec);
  if(pmath_is_null(result)){
    pmath_unref(floatA);
    pmath_unref(floatB);
    return PMATH_NULL;
  }
  
  // d(x+y) = dx + dy
  mpfr_add(
    PMATH_AS_MP_ERROR(result), 
    PMATH_AS_MP_ERROR(floatA),
    PMATH_AS_MP_ERROR(floatB), 
    MPFR_RNDU);
    
  mpfr_add(
    PMATH_AS_MP_VALUE(result),
    PMATH_AS_MP_VALUE(floatA),
    PMATH_AS_MP_VALUE(floatB),
    MPFR_RNDN);
  
  _pmath_mp_float_normalize(result);
  
  pmath_unref(floatA);
  pmath_unref(floatB);
  return _pmath_float_exceptions(result);
}

static pmath_t _add_mi(
  pmath_float_t   floatA, // will be freed. not PMATH_NULL!
  pmath_integer_t intB    // will be freed. not PMATH_NULL!
){
  double d;
  
  assert(pmath_is_double(floatA));
  
  d = PMATH_AS_DOUBLE(floatA)+ mpz_get_d(PMATH_AS_MPZ(intB));
  if(!isfinite(d))
    return _add_nn(_pmath_convert_to_mp_float(floatA), intB);
  
  pmath_unref(intB);
  pmath_unref(floatA);
  return pmath_float_new_d(d);
}

static pmath_t _add_mq(
  pmath_float_t    floatA, // will be freed. not PMATH_NULL!
  pmath_quotient_t quotB   // will be freed. not PMATH_NULL!
){
  double d;
  
  assert(pmath_is_double(floatA));
  assert(pmath_is_quotient(quotB));
  
  d = mpz_get_d(PMATH_AS_MPZ(PMATH_QUOT_NUM(quotB)));
  d/= mpz_get_d(PMATH_AS_MPZ(PMATH_QUOT_DEN(quotB)));
  
  d+= PMATH_AS_DOUBLE(floatA);
  
  if(!isfinite(d))
    return _add_nn(_pmath_convert_to_mp_float(floatA), quotB);
  
  pmath_unref(quotB);
  pmath_unref(floatA);
  return pmath_float_new_d(d);
}

static pmath_t _add_mf(
  pmath_float_t  floatA, // will be freed. not PMATH_NULL!
  pmath_float_t  floatB  // will be freed. not PMATH_NULL!
){
  double d;
  
  assert(pmath_is_double(floatA));
  assert(pmath_is_mpfloat(floatB));
  
  d = PMATH_AS_DOUBLE(floatA) + mpfr_get_d(PMATH_AS_MP_VALUE(floatB), MPFR_RNDN);
  
  if(!isfinite(d))
    return _add_nn(_pmath_convert_to_mp_float(floatA), floatB);
  
  pmath_unref(floatB);
  pmath_unref(floatA);
  return pmath_float_new_d(d);
}

static pmath_t _add_mm(
  pmath_float_t  floatA, // will be freed. not PMATH_NULL!
  pmath_float_t  floatB  // will be freed. not PMATH_NULL!
){
  double d;
  
  assert(pmath_is_double(floatA));
  assert(pmath_is_double(floatB));
  
  d = PMATH_AS_DOUBLE(floatA) + PMATH_AS_DOUBLE(floatB);
          
  if(!isfinite(d)){
    return _add_nn(
      _pmath_convert_to_mp_float(floatA), 
      _pmath_convert_to_mp_float(floatB));
  }
  
  pmath_unref(floatA);
  pmath_unref(floatB);
  return pmath_float_new_d(d);
}

PMATH_PRIVATE pmath_t _add_nn(
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
      return _add_mm(numA, numB);
    
    assert(pmath_is_pointer(numB));
    
    switch(PMATH_AS_PTR(numB)->type_shift){
      case PMATH_TYPE_SHIFT_INTEGER:        return _add_mi(numA, numB);
      case PMATH_TYPE_SHIFT_QUOTIENT:       return _add_mq(numA, numB);
      case PMATH_TYPE_SHIFT_MP_FLOAT:       return _add_mf(numA, numB);
    }
    
    assert("invalid number type" && 0);
    return PMATH_NULL;
  }
  
  if(pmath_is_double(numB))
    return _add_nn(numB, numA);
  
  assert(pmath_is_pointer(numA));
  
  switch(PMATH_AS_PTR(numA)->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER: {
      switch(PMATH_AS_PTR(numB)->type_shift){
        case PMATH_TYPE_SHIFT_INTEGER:        return _add_ii(numA, numB);
        case PMATH_TYPE_SHIFT_QUOTIENT:       return _add_qi(numB, numA);
        case PMATH_TYPE_SHIFT_MP_FLOAT:       return _add_fi(numB, numA);
      }
    } break;
    
    case PMATH_TYPE_SHIFT_QUOTIENT: {
      switch(PMATH_AS_PTR(numB)->type_shift){
        case PMATH_TYPE_SHIFT_INTEGER:        return _add_qi(numA, numB);
        case PMATH_TYPE_SHIFT_QUOTIENT:       return _add_qq(numA, numB);
        case PMATH_TYPE_SHIFT_MP_FLOAT:       return _add_fq(numB, numA);
      }
    } break;
    
    case PMATH_TYPE_SHIFT_MP_FLOAT: {
      switch(PMATH_AS_PTR(numB)->type_shift){
        case PMATH_TYPE_SHIFT_INTEGER:        return _add_fi(numA, numB);
        case PMATH_TYPE_SHIFT_QUOTIENT:       return _add_fq(numA, numB);
        case PMATH_TYPE_SHIFT_MP_FLOAT:       return _add_ff(numA, numB);
      }
    } break;
  }
  
  assert("invalid number type" && 0);
  
  return PMATH_NULL;
}

PMATH_PRIVATE void split_summand(
  pmath_t  summand,         // wont be freed
  pmath_t *out_num_factor,
  pmath_t *out_rest
){
  if(pmath_is_expr(summand)){
    pmath_t head = pmath_expr_get_item(summand, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_SYMBOL_TIMES)){
      size_t len = pmath_expr_length(summand);
      
      if(len > 1){
        pmath_t first = pmath_expr_get_item(summand, 1);
        if(pmath_is_number(first)
        || _pmath_is_nonreal_complex(first)){
          *out_num_factor = first;
          
          if(len == 2){
            *out_rest = pmath_expr_get_item(summand, 2);
            return;
          }
          
          *out_rest = pmath_expr_get_item_range(summand, 2, len-1);
          return;
        }
        pmath_unref(first);
      }
      
      *out_num_factor = pmath_integer_new_ui(1);
      *out_rest = pmath_ref(summand);
      return;
    }
    
    if(_pmath_is_nonreal_complex(summand)){
      *out_num_factor = pmath_ref(summand);
      *out_rest = PMATH_UNDEFINED;
      return;
    }
  }
  
  if(pmath_is_number(summand)){
    *out_num_factor = pmath_ref(summand);
    *out_rest = PMATH_UNDEFINED;
    return;
  }
  
  *out_num_factor = pmath_integer_new_ui(1);
  *out_rest = pmath_ref(summand);
}

static void plus_2_arg(pmath_t *a, pmath_t *b){
  if(pmath_is_number(*a)){
    if(pmath_is_number(*b)){
      *a = _add_nn(*a, *b);
      *b = PMATH_UNDEFINED;
      return;
    }

    if(pmath_equals(*a, PMATH_NUMBER_ZERO)){
      pmath_unref(*a);
      *a = *b;
      *b = PMATH_UNDEFINED;
      return;
    }
    
    if(_pmath_is_nonreal_complex(*b)){
      *a = pmath_expr_set_item(*b, 1,
        _add_nn(
          *a,
          pmath_expr_get_item(*b, 1)));
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
    
    if(pmath_is_real(*a) && _pmath_is_numeric(*b)){
      if(pmath_compare(*a, PMATH_NUMBER_ZERO) == 0)
        *b = pmath_approximate(*b, HUGE_VAL, pmath_accuracy(pmath_ref(*a)));
      else
        *b = pmath_approximate(*b, pmath_precision(pmath_ref(*a)), HUGE_VAL);
      return;
    }
  }
  else if(_pmath_is_nonreal_complex(*a)){
    if(pmath_is_number(*b)){
      *a = pmath_expr_set_item(*a, 1,
        _add_nn(
          *b,
          pmath_expr_get_item(*a, 1)));
      *b = PMATH_UNDEFINED;
      return;
    }
    
    if(_pmath_is_nonreal_complex(*b)){
      pmath_t re = _add_nn(
        pmath_expr_get_item(*a, 1),
        pmath_expr_get_item(*b, 1));
      pmath_t im = _add_nn(
        pmath_expr_get_item(*a, 2),
        pmath_expr_get_item(*b, 2));
      pmath_unref(*b);
      *b = PMATH_UNDEFINED;
      *a = pmath_expr_set_item(*a, 1, re);
      *a = pmath_expr_set_item(*a, 2, im);
      return;
    }
    
    if(_pmath_is_inexact(*a)){
      *b = pmath_approximate(*b, pmath_precision(pmath_ref(*a)), HUGE_VAL);
      return;
    }
  }
  else if(_pmath_is_nonreal_complex(*b)){
    if(_pmath_is_inexact(*b)){
      *a = pmath_approximate(*a, pmath_precision(pmath_ref(*b)), HUGE_VAL);
      return;
    }
  }
  
  if(pmath_equals(*a, _pmath_object_overflow)
  && (pmath_equals(*b, _pmath_object_overflow)
   || pmath_equals(*b, _pmath_object_underflow)
   || _pmath_is_numeric(*b)))
  {
    pmath_unref(*b);
    *b = PMATH_UNDEFINED;
  }
  
  if(pmath_equals(*b, _pmath_object_overflow) && _pmath_is_numeric(*a)){
    pmath_unref(*a);
    *a = *b;
    *b = PMATH_UNDEFINED;
  }
  
  if(pmath_equals(*a, _pmath_object_underflow) && _pmath_is_numeric(*b)){
    pmath_unref(*b);
    *b = PMATH_UNDEFINED;
  }
  
  {
    pmath_t ainfdir = _pmath_directed_infinity_direction(*a);
    pmath_t binfdir = _pmath_directed_infinity_direction(*b);
    if(!pmath_is_null(ainfdir) && !pmath_is_null(binfdir)){
      pmath_t sum;

      if(pmath_equals(ainfdir, binfdir)){
        pmath_unref(ainfdir);
        pmath_unref(binfdir);
        pmath_unref(*b);
        *b = PMATH_UNDEFINED;
        return;
      }

      sum = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PLUS), 2,
          ainfdir,
          binfdir));

      if(pmath_equals(sum, PMATH_NUMBER_ZERO)){
        pmath_unref(sum);
        pmath_message(PMATH_SYMBOL_INFINITY, "indet", 1,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_PLUS), 2,
            *a,
            *b));
        *a = pmath_ref(PMATH_SYMBOL_UNDEFINED);
        *b = PMATH_UNDEFINED;
      }
      return;
    }
    
    if(!pmath_is_null(ainfdir)){
      pmath_unref(ainfdir);
      pmath_unref(*b);
      *b = PMATH_UNDEFINED;
      return;
    }
    
    if(pmath_is_null(binfdir)){
      pmath_unref(binfdir);
      pmath_unref(*a);
      *a = *b;
      *b = PMATH_UNDEFINED;
      return;
    }
  }

  {
    pmath_number_t numFactorA;
    pmath_number_t numFactorB;
    pmath_t restA;
    pmath_t restB;
    split_summand(*a, &numFactorA, &restA);
    split_summand(*b, &numFactorB, &restB);

    if(pmath_equals(restA, restB)){
      pmath_unref(*a);
      pmath_unref(*b);
      pmath_unref(restB);
      *a = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TIMES), 2,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PLUS), 2,
          numFactorA,
          numFactorB),
        restA);
      *b = PMATH_UNDEFINED;
      return;
    }

    pmath_unref(numFactorA);
    pmath_unref(numFactorB);
    pmath_unref(restA);
    pmath_unref(restB);
  }
}

PMATH_PRIVATE pmath_t builtin_plus(pmath_expr_t expr){
  size_t ia, ib;
  const size_t elen = pmath_expr_length(expr);
  
  if(elen == 0){
    pmath_unref(expr);
    return pmath_integer_new_ui(0);
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
