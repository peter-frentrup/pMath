#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>

#include <pmath-core/packed-arrays-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>


PMATH_PRIVATE pmath_t builtin_internal_realballfrommidpointradius(pmath_expr_t expr) {
  /* Internal`RealBallFromMidpointRadius(mid, rad, WorkingPrecision -> Automatic)
     Internal`RealBallFromMidpointRadius({mid, rad})
   */
  pmath_number_t mid, rad;
  pmath_t obj, options;
  size_t exprlen = pmath_expr_length(expr);
  size_t last_nonoption;
  slong precision;
  
  if(exprlen < 1) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_LIST, 2)) {
    mid = pmath_expr_get_item(obj, 1);
    rad = pmath_expr_get_item(obj, 2);
    pmath_unref(obj);
    if(!pmath_is_number(mid) || !pmath_is_number(rad)) {
      pmath_unref(mid);
      pmath_unref(rad);
      pmath_message(PMATH_NULL, "numpa", 2, INT(1), pmath_ref(expr));
      return expr;
    }
    last_nonoption = 1;
  }
  else if(pmath_is_number(obj)) {
    mid = obj;
    rad = pmath_expr_get_item(expr, 2);
    if(!pmath_is_number(rad)) {
      pmath_unref(mid);
      pmath_unref(rad);
      pmath_message(PMATH_NULL, "num", 2, INT(2), pmath_ref(expr));
      return expr;
    }
    last_nonoption = 2;
  }
  else {
    pmath_unref(obj);
    pmath_message(PMATH_NULL, "numpa", 2, INT(1), pmath_ref(expr));
    return expr;
  }
  
  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options)) {
    pmath_unref(mid);
    pmath_unref(rad);
    return expr;
  }
  
  obj = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_WORKINGPRECISION, options);
  pmath_unref(options);
  if(pmath_same(obj, PMATH_SYMBOL_AUTOMATIC)) {
    if(pmath_is_mpfloat(mid)) {
      precision = PMATH_AS_ARB_WORKING_PREC(mid);
    }
    else if(pmath_is_double(mid)) {
      precision = DBL_MANT_DIG;
    }
    else if(pmath_is_mpfloat(rad)) {
      precision = PMATH_AS_ARB_WORKING_PREC(rad);
    }
    else if(pmath_is_double(rad)) {
      precision = DBL_MANT_DIG;
    }
    else {
      pmath_message(PMATH_NULL, "prec", 2, mid, rad);
      pmath_unref(obj);
      return expr;
    }
  }
  else {
    double prec;
    if(!_pmath_to_precision(obj, &prec) || prec < MPFR_PREC_MIN || prec > PMATH_MP_PREC_MAX){
      pmath_message(PMATH_NULL, "invprec", 1, obj);
      pmath_unref(mid);
      pmath_unref(rad);
      return expr;
    }
    precision = (slong)prec;
  }
  pmath_unref(obj);
  
  obj = PMATH_NULL;
  pmath_unref(expr);
  if(pmath_is_mpfloat(mid) && pmath_is_mpfloat(rad)) {
    obj = _pmath_create_mp_float_from_midrad_arb(PMATH_AS_ARB(mid), PMATH_AS_ARB(rad), (slong)precision);
  }
  else {
    arb_t a_mid;
    arb_t a_rad;
    
    arb_init(a_mid);
    arb_init(a_rad);
    
    _pmath_number_get_arb(a_mid, mid, (slong)precision);
    _pmath_number_get_arb(a_rad, rad, (slong)precision);
    
    obj = _pmath_create_mp_float_from_midrad_arb(a_mid, a_rad, (slong)precision);
    
    arb_clear(a_rad);
    arb_clear(a_mid);
  }
  
  pmath_unref(mid);
  pmath_unref(rad);
  return obj;
}

PMATH_PRIVATE pmath_t builtin_internal_realballmidpointradius(pmath_expr_t expr) {
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_is_mpfloat(x)) {
    pmath_mpfloat_t mid = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(x));
    pmath_mpfloat_t rad = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(x));
    pmath_unref(expr);
    if(pmath_is_null(mid) || pmath_is_null(rad)) {
      pmath_unref(mid);
      pmath_unref(rad);
      return PMATH_NULL;
    }
    
    arf_set(    arb_midref(PMATH_AS_ARB(mid)), arb_midref(PMATH_AS_ARB(x)));
    arf_set_mag(arb_midref(PMATH_AS_ARB(rad)), arb_radref(PMATH_AS_ARB(x)));
    mag_zero(arb_radref(PMATH_AS_ARB(mid)));
    mag_zero(arb_radref(PMATH_AS_ARB(rad)));
    
    pmath_unref(x);
    return pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 2, mid, rad);
  }
  
  if(pmath_is_double(x)) {
    size_t len = 2;
    pmath_packed_array_t arr = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_DOUBLE, 1, &len, NULL, 0);
    double *data = pmath_packed_array_begin_write(&arr, NULL, 0);
    if(!data)
      return expr;
      
    pmath_unref(expr);
    data[0] = PMATH_AS_DOUBLE(x);
    data[1] = 0.0;
    return arr;
  }
  
  if(pmath_is_int32(x)) {
    size_t len = 2;
    pmath_packed_array_t arr = pmath_packed_array_new(PMATH_NULL, PMATH_PACKED_INT32, 1, &len, NULL, 0);
    double *data = pmath_packed_array_begin_write(&arr, NULL, 0);
    if(!data)
      return expr;
      
    pmath_unref(expr);
    data[0] = PMATH_AS_INT32(x);
    data[1] = 0;
    return arr;
  }
  
  if(pmath_is_number(x)) {
    pmath_unref(expr);
    return pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 2, x, PMATH_FROM_INT32(0));
  }
  
  if(pmath_same(x, PMATH_SYMBOL_UNDEFINED)) {
    pmath_unref(expr);
    return pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 2, x, pmath_ref(x));
  }
  
  if(_pmath_is_infinite(x)) {
    pmath_t infdir = _pmath_directed_infinity_direction(x);
    
    if(pmath_same(infdir, PMATH_FROM_INT32(-1)) || pmath_same(infdir, PMATH_FROM_INT32(1))) {
      pmath_unref(expr);
      return pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 2, x, PMATH_FROM_INT32(0));
    }
    
    pmath_unref(infdir);
    /* DirectedInfinity(0) represents a complex infinity, not only a real infinity */
    //pmath_unref(expr);
    //return pmath_expr_new_extended(
    //         pmath_ref(PMATH_SYMBOL_LIST), 2,
    //         x,
    //         pmath_ref(_pmath_object_pos_infinity));
    pmath_unref(x);
    return expr;
  }
  
  pmath_unref(x);
  return expr;
}
