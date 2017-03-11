#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/messages.h>


PMATH_PRIVATE pmath_t builtin_internal_realballbounds(pmath_expr_t expr) {
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_is_mpfloat(x)) {
    pmath_mpfloat_t lower = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(x));
    pmath_mpfloat_t upper = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(x));
    pmath_unref(expr);
    if(pmath_is_null(lower) || pmath_is_null(upper)) {
      pmath_unref(lower);
      pmath_unref(upper);
      return PMATH_NULL;
    }
    
    _pmath_arb_bounds(
      arb_midref(PMATH_AS_ARB(lower)), 
      arb_midref(PMATH_AS_ARB(upper)), 
      PMATH_AS_ARB(x), 
      ARF_PREC_EXACT);
    mag_zero(arb_radref(PMATH_AS_ARB(lower)));
    mag_zero(arb_radref(PMATH_AS_ARB(upper)));
    
    arf_get_mpfr(PMATH_AS_MP_VALUE(lower), arb_midref(PMATH_AS_ARB(lower)), MPFR_RNDN);
    arf_get_mpfr(PMATH_AS_MP_VALUE(upper), arb_midref(PMATH_AS_ARB(upper)), MPFR_RNDN);
    
    pmath_unref(x);
    return pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 2, lower, upper);
  }
  
  if(pmath_is_double(x)) {
    size_t len = 2;
    pmath_packed_array_t arr = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_DOUBLE, 1, &len, NULL, 0);
    double *data = pmath_packed_array_begin_write(&arr, NULL, 0);
    if(!data)
      return expr;
      
    pmath_unref(expr);
    data[0] = data[1] = PMATH_AS_DOUBLE(x);
    return arr;
  }
  
  if(pmath_is_int32(x)) {
    size_t len = 2;
    pmath_packed_array_t arr = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_INT32, 1, &len, NULL, 0);
    double *data = pmath_packed_array_begin_write(&arr, NULL, 0);
    if(!data)
      return expr;
      
    pmath_unref(expr);
    data[0] = data[1] = PMATH_AS_INT32(x);
    return arr;
  }
  
  if(pmath_is_number(x) || pmath_same(x, PMATH_SYMBOL_UNDEFINED)) {
    pmath_unref(expr);
    return pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 2, x, pmath_ref(x));
  }
  
  if(_pmath_is_infinite(x)) {
    pmath_t infdir = _pmath_directed_infinity_direction(x);
    
    if(pmath_same(infdir, PMATH_FROM_INT32(-1)) || pmath_same(infdir, PMATH_FROM_INT32(1))) {
      pmath_unref(expr);
      return pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 2, x, pmath_ref(x));
    }
    
    pmath_unref(infdir);
    /* DirectedInfinity(0) represents a complex infinity, not only a real infinity */
    //pmath_unref(x);
    //pmath_unref(expr);
    //return pmath_expr_new_extended(
    //         pmath_ref(PMATH_SYMBOL_LIST), 2, 
    //         pmath_ref(_pmath_object_neg_infinity), 
    //         pmath_ref(_pmath_object_pos_infinity));
    pmath_unref(x);
    return expr;
  }
  
  pmath_unref(x);
  return expr;
}
