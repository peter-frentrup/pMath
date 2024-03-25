#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/build-expr-private.h>


extern pmath_symbol_t pmath_System_Power;
extern pmath_symbol_t pmath_System_Times;
extern pmath_symbol_t pmath_System_Undefined;

static pmath_t div_nn(pmath_t expr, pmath_number_t num, pmath_number_t den);
static pmath_t div_ff(pmath_mpfloat_t num, pmath_mpfloat_t den);
static pmath_t div_fi(pmath_mpfloat_t num, pmath_mpint_t den);
static pmath_t div_f_si(pmath_mpfloat_t num, int32_t den);
static pmath_t div_si_f(int32_t num, pmath_mpfloat_t den);


PMATH_PRIVATE pmath_t builtin_divide(pmath_expr_t expr) {
  /* Divide(num, den)
   */
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  pmath_t num = pmath_expr_get_item(expr, 1);
  pmath_t den = pmath_expr_get_item(expr, 2);
  
  if(pmath_is_number(num)) {
    if(pmath_is_number(den))
      return div_nn(expr, num, den);
  }
  
  // TODO: special case for complex numbers ...
  
  pmath_unref(expr);
  return DIV(num, den);
}

static pmath_t div_nn(pmath_t expr, pmath_number_t num, pmath_number_t den) { // arguments will be freed
  if(pmath_is_int32(den)) {
    int32_t i_den = PMATH_AS_INT32(den);
    
    if(PMATH_UNLIKELY(i_den == 0)) {
      if(pmath_number_sign(num) == 0) {
        pmath_unref(num);
        pmath_message(PMATH_NULL, "indet", 1, expr);
        return pmath_ref(pmath_System_Undefined);
      }
      
      pmath_unref(num);
      pmath_message(PMATH_NULL, "infy", 1, expr);
      return CINFTY;
    }
    
    pmath_unref(expr); expr = PMATH_NULL;
    
    if(PMATH_UNLIKELY(i_den == 1))
      return num;
    
    if(pmath_is_double(num)) {
      double q = PMATH_AS_DOUBLE(num) / i_den;
      
      if(isfinite(q))
        return PMATH_FROM_DOUBLE(q);
    }
    else if(pmath_is_mpfloat(num))
      return div_f_si(num, i_den);
    
    if(PMATH_UNLIKELY(i_den == -1))
      return pmath_number_neg(num);
    
    if(pmath_is_int32(num)) {
      int32_t i_num = PMATH_AS_INT32(num);
      
      if(i_num % i_den == 0)
        return PMATH_FROM_INT32(i_num / i_den); // cannot overflow because i_den != -1
    }
    
    if(pmath_is_integer(num))
      return pmath_rational_new(num, den);
  }
  else if(pmath_is_double(den)) {
    if(PMATH_AS_DOUBLE(den) == 0) {
      if(pmath_number_sign(num) == 0) {
        pmath_unref(num);
        pmath_message(PMATH_NULL, "indet", 1, expr);
        return pmath_ref(pmath_System_Undefined);
      }
      
      pmath_unref(num);
      pmath_message(PMATH_NULL, "infy", 1, expr);
      return CINFTY;
    }
    
    pmath_unref(expr); expr = PMATH_NULL;
    
    if(pmath_is_double(num)) {
      double q = PMATH_AS_DOUBLE(num) / PMATH_AS_DOUBLE(den);
      
      if(isfinite(q))
        return PMATH_FROM_DOUBLE(q);
    }
    else if(pmath_is_int32(num)) {
      double q = PMATH_AS_INT32(num) / PMATH_AS_DOUBLE(den);
      
      if(isfinite(q))
        return PMATH_FROM_DOUBLE(q);
    }
    else {
      double d_num = pmath_number_get_d(num);
      double q = d_num / PMATH_AS_DOUBLE(den);
      
      if(isfinite(q)) {
        pmath_unref(num);
        return PMATH_FROM_DOUBLE(q);
      }
    }
  }
  else if(pmath_is_mpfloat(den)) {
    if(arb_contains_zero(PMATH_AS_ARB(den))) {
      pmath_unref(den);
      if(pmath_number_sign(num) == 0) {
        pmath_unref(num);
        pmath_message(PMATH_NULL, "indet", 1, expr);
        return pmath_ref(pmath_System_Undefined);
      }
      
      pmath_unref(num);
      pmath_message(PMATH_NULL, "infy", 1, expr);
      return CINFTY;
    }
    
    pmath_unref(expr); expr = PMATH_NULL;
    
    if(pmath_is_mpfloat(num)) 
      return div_ff(num, den);
    
    if(pmath_is_int32(num))
      return div_si_f(PMATH_AS_INT32(num), den);
    
    if(pmath_is_double(num)) {
      double d_den = arf_get_d(arb_midref(PMATH_AS_ARB(den)), ARF_RND_NEAR);
      double q = PMATH_AS_DOUBLE(num) / d_den;
      
      if(isfinite(q)) {
        pmath_unref(den);
        return PMATH_FROM_DOUBLE(q);
      }
    }
  }
  else if(pmath_is_mpint(den)) { // Not 0, because that would be a small int (int32_t), checked before.
    pmath_unref(expr); expr = PMATH_NULL;
    
    if(pmath_is_integer(num))
      return pmath_rational_new(num, den);
      
    if(pmath_is_mpfloat(num))
      return div_fi(num, den);
      
    if(pmath_is_double(num)) {
      double d_den = mpz_get_d(PMATH_AS_MPZ(den));
      double q = PMATH_AS_DOUBLE(num) / d_den;
      
      if(isfinite(q)) {
        pmath_unref(den);
        return PMATH_FROM_DOUBLE(q);
      }
    }
  }
  
  // TODO: special cases for quotients
  
  pmath_unref(expr); expr = PMATH_NULL;
  return DIV(num, den);
}

static pmath_t div_ff(pmath_mpfloat_t num, pmath_mpfloat_t den) {
  pmath_mpfloat_t result;
  slong prec;
  
  assert(pmath_is_mpfloat(num));
  assert(pmath_is_mpfloat(den));
  
  prec = FLINT_MAX(PMATH_AS_ARB_WORKING_PREC(num), PMATH_AS_ARB_WORKING_PREC(den));
  result = _pmath_create_mp_float(prec);
  if(pmath_is_null(result)) {
    pmath_unref(num);
    pmath_unref(den);
    return PMATH_NULL;
  }
  
  arb_div(PMATH_AS_ARB(result), PMATH_AS_ARB(num), PMATH_AS_ARB(den), prec);
    
  pmath_unref(num);
  pmath_unref(den);
  return _pmath_float_exceptions(result);
}

static pmath_t div_fi(pmath_mpfloat_t num, pmath_mpint_t den) {
  pmath_mpfloat_t result;
  slong prec;
  
  assert(pmath_is_mpfloat(num));
  assert(pmath_is_mpint(den));
  
  prec = PMATH_AS_ARB_WORKING_PREC(num);
  result = _pmath_create_mp_float(prec);
  if(pmath_is_null(result)) {
    pmath_unref(num);
    pmath_unref(den);
    return PMATH_NULL;
  }
  
  fmpz_t z_den;
  fmpz_init_set_readonly(z_den, PMATH_AS_MPZ(den));
  arb_div_fmpz(PMATH_AS_ARB(result), PMATH_AS_ARB(num), z_den, prec);
  fmpz_clear_readonly(z_den);
    
  pmath_unref(num);
  pmath_unref(den);
  return _pmath_float_exceptions(result);
}

static pmath_t div_f_si(pmath_mpfloat_t num, int32_t den) {
  pmath_mpfloat_t result;
  slong prec;
  
  assert(pmath_is_mpfloat(num));
  
  prec = PMATH_AS_ARB_WORKING_PREC(num);
  result = _pmath_create_mp_float(prec);
  if(pmath_is_null(result)) {
    pmath_unref(num);
    return PMATH_NULL;
  }
  
  arb_div_si(PMATH_AS_ARB(result), PMATH_AS_ARB(num), den, prec);
    
  pmath_unref(num);
  return _pmath_float_exceptions(result);
}

static pmath_t div_si_f(int32_t num, pmath_mpfloat_t den) {
  pmath_mpfloat_t result;
  slong prec;
  
  assert(pmath_is_mpfloat(den));
  
  prec = PMATH_AS_ARB_WORKING_PREC(den);
  result = _pmath_create_mp_float(prec);
  if(pmath_is_null(result)) {
    pmath_unref(den);
    return PMATH_NULL;
  }
  
  if(num < 0) {
    arb_ui_div(PMATH_AS_ARB(result), -(uint32_t)num, PMATH_AS_ARB(den), prec);
    arb_neg(PMATH_AS_ARB(result), PMATH_AS_ARB(result));
  }
  else
    arb_ui_div(PMATH_AS_ARB(result), (uint32_t)num, PMATH_AS_ARB(den), prec);
    
  pmath_unref(den);
  return _pmath_float_exceptions(result);
}
