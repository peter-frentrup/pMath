#include <pmath-util/approximate.h>

#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>


static pmath_t approx_const_generic(
  double prec, 
  int (*generator)(mpfr_ptr, mpfr_rnd_t),
  double double_value
) {
  pmath_mpfloat_t result;
  mpfr_rnd_t rnd = _pmath_current_rounding_mode();
  
  if(prec == -HUGE_VAL)
    return PMATH_FROM_DOUBLE(double_value);
    
  if(prec > PMATH_MP_PREC_MAX) {
    pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
    return pmath_ref(_pmath_object_overflow);
  }
  
  if(prec < MPFR_PREC_MIN)
    prec = MPFR_PREC_MIN;
    
  result = _pmath_create_mp_float((mpfr_prec_t)round(prec));
  if(pmath_is_null(result))
    return PMATH_NULL;
  
  generator(PMATH_AS_MP_VALUE(result), rnd);
  
  return result;
}

static int mpfr_const_exp1(mpfr_ptr rop, mpfr_rnd_t rnd) {
  mpfr_set_ui(rop, 1, rnd);
  return mpfr_exp(rop, rop, rnd);
}

static int mpfr_const_machineprecision(mpfr_ptr rop, mpfr_rnd_t rnd) {
  MPFR_DECL_INIT(two, DBL_MANT_DIG);
  
  mpfr_set_ui(two, 2, rnd);
  mpfr_log10(rop, two, rnd);
  return mpfr_mul_ui(rop, rop, DBL_MANT_DIG, rnd);
}

PMATH_PRIVATE pmath_bool_t builtin_approximate_e(pmath_t *obj, double prec) {
  if(!pmath_same(*obj, PMATH_SYMBOL_E) || prec == HUGE_VAL)
    return FALSE;
  
  pmath_unref(*obj);
  *obj = approx_const_generic(prec, mpfr_const_exp1, M_E);
  return TRUE;
}

PMATH_PRIVATE pmath_bool_t builtin_approximate_eulergamma(pmath_t *obj, double prec) {
  if(!pmath_same(*obj, PMATH_SYMBOL_EULERGAMMA) || prec == HUGE_VAL)
    return FALSE;
  
  pmath_unref(*obj);
  *obj = approx_const_generic(prec, mpfr_const_euler, 0.57721566490153286061);
  return TRUE;
}

PMATH_PRIVATE pmath_bool_t builtin_approximate_machineprecision(pmath_t *obj, double prec) {
  if(!pmath_same(*obj, PMATH_SYMBOL_MACHINEPRECISION) || prec == HUGE_VAL)
    return FALSE;
  
  pmath_unref(*obj);
  *obj = approx_const_generic(prec, mpfr_const_machineprecision, (double)LOG10_2 * DBL_MANT_DIG);
  return TRUE;
}

PMATH_PRIVATE pmath_bool_t builtin_approximate_pi(pmath_t *obj, double prec) {
  if(!pmath_same(*obj, PMATH_SYMBOL_PI) || prec == HUGE_VAL)
    return FALSE;
    
  pmath_unref(*obj);
  *obj = approx_const_generic(prec, mpfr_const_pi, M_PI);
  return TRUE;
}
