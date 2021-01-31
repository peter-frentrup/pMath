#include <pmath-util/approximate.h>

#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>


extern pmath_symbol_t pmath_System_E;
extern pmath_symbol_t pmath_System_EulerGamma;
extern pmath_symbol_t pmath_System_General;
extern pmath_symbol_t pmath_System_MachinePrecision;
extern pmath_symbol_t pmath_System_Pi;

static pmath_t approx_const_generic(
  double prec,
  void (*generator)(arb_t, slong),
  double double_value
) {
  pmath_mpfloat_t result;
  
  if(prec == -HUGE_VAL)
    return PMATH_FROM_DOUBLE(double_value);
  
  if(prec > PMATH_MP_PREC_MAX) {
    pmath_message(pmath_System_General, "ovfl", 0);
    return pmath_ref(_pmath_object_overflow);
  }
  
  if(prec < MPFR_PREC_MIN)
    prec = MPFR_PREC_MIN;
  
  result = _pmath_create_mp_float((slong)ceil(prec));
  if(pmath_is_null(result))
    return PMATH_NULL;
    
  generator(PMATH_AS_ARB(result), PMATH_AS_ARB_WORKING_PREC(result));
  return result;
}

static void arb_const_machineprecision(arb_t rop, slong prec) {
  arb_t log10;
  arb_t log2;
  arb_init(log10);
  arb_init(log2);
  
  arb_const_log10(log10, prec);
  arb_const_log2(log2, prec);
  arb_set_ui(rop, DBL_MANT_DIG);
  arb_mul(rop, rop, log2, prec);
  arb_div(rop, rop, log10, prec);
  
  arb_clear(log10);
  arb_clear(log2);
}

PMATH_PRIVATE pmath_bool_t builtin_approximate_e(pmath_t *obj, double prec) {
  if(!pmath_same(*obj, pmath_System_E) || prec == HUGE_VAL)
    return FALSE;
    
  pmath_unref(*obj);
  *obj = approx_const_generic(prec, arb_const_e, M_E);
  return TRUE;
}

PMATH_PRIVATE pmath_bool_t builtin_approximate_eulergamma(pmath_t *obj, double prec) {
  if(!pmath_same(*obj, pmath_System_EulerGamma) || prec == HUGE_VAL)
    return FALSE;
    
  pmath_unref(*obj);
  *obj = approx_const_generic(prec, arb_const_euler, 0.57721566490153286061);
  
  return TRUE;
}

PMATH_PRIVATE pmath_bool_t builtin_approximate_machineprecision(pmath_t *obj, double prec) {
  if(!pmath_same(*obj, pmath_System_MachinePrecision) || prec == HUGE_VAL)
    return FALSE;
    
  pmath_unref(*obj);
  *obj = approx_const_generic(prec, arb_const_machineprecision, (double)LOG10_2 * DBL_MANT_DIG);
    
  return TRUE;
}

PMATH_PRIVATE pmath_bool_t builtin_approximate_pi(pmath_t *obj, double prec) {
  if(!pmath_same(*obj, pmath_System_Pi) || prec == HUGE_VAL)
    return FALSE;
    
  pmath_unref(*obj);
  *obj = approx_const_generic(prec, arb_const_pi, M_PI);
    
  return TRUE;
}
