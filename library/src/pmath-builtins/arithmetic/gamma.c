#include <pmath-core/numbers-private.h>
#include <pmath-core/intervals-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>

static pmath_integer_t factorial(unsigned long n) {
  if(n == 0)
    return INT(1);
    
  if(n <= 86180310) { // Log(10, 86180310!) >= 2^31
    pmath_mpint_t result = _pmath_create_mp_int(0);
    
    if(!pmath_is_null(result)) {
      mpz_set_ui(PMATH_AS_MPZ(result), n);
      
      while(--n > 0 && !pmath_aborting()) {
        mpz_mul_ui(PMATH_AS_MPZ(result), PMATH_AS_MPZ(result), n);
      }
      
      return _pmath_mp_int_normalize(result);
    }
  }
  
  return PMATH_NULL;
}

static pmath_integer_t double_factorial(unsigned long n) {
  if(n <= 1)
    return INT(1);
    
  if(n <= 166057019) { // 166057019!! >= 2^31
    pmath_mpint_t result = _pmath_create_mp_int(0);
    
    if(!pmath_is_null(result)) {
      mpz_set_ui(PMATH_AS_MPZ(result), n);
      
      while(n > 2 && !pmath_aborting()) {
        n -= 2;
        mpz_mul_ui(PMATH_AS_MPZ(result), PMATH_AS_MPZ(result), n);
      }
      
      return _pmath_mp_int_normalize(result);
    }
  }
  
  return PMATH_NULL;
}

static pmath_t gamma_d(double z) {
  pmath_mpfloat_t tmp = _pmath_create_mp_float_from_d(z);
  tmp = _pmath_mpfloat_call(tmp, mpfr_gamma);
  
  if(pmath_is_mpfloat(tmp)) {
    double z = mpfr_get_d(PMATH_AS_MP_VALUE(tmp), MPFR_RNDN);
    
    pmath_unref(tmp);
    if(!isfinite(z)) {
      pmath_message(PMATH_NULL, "ovfl", 0);
      return pmath_ref(_pmath_object_overflow);
    }
    
    return PMATH_FROM_DOUBLE(z);
  }
  
  return tmp;
}

// MPFI convention is to return flags indicating which endpoint was inexact. We ignore this!
static int _mpfi_gamma(mpfi_ptr result, mpfi_srcptr x) {
  mpfi_t ival, tmp; /* The positive zero psi_zero of digamma is always in ival. */
  mpfr_t val;
  mpfr_prec_t prec;
  pmath_bool_t result_is_positive;
  pmath_bool_t has_infinite_endpoint;
  
  if(mpfi_nan_p(x)) {
    mpfr_set_nan(&result->left);
    mpfr_set_nan(&result->right);
    mpfr_set_nanflag();
    return 0;
  }
  
  if(mpfr_equal_p(&x->left, &x->right)) {
    mpfr_gamma(&result->left,  &x->left, MPFR_RNDD);
    mpfr_gamma(&result->right, &x->left, MPFR_RNDU);
    return 0; // 3
  }
  
  prec = mpfi_get_prec(result);
  mpfi_init2(ival, 10 + prec);
  
  if(mpfi_is_pos(x)) {
    result_is_positive = TRUE;
    has_infinite_endpoint = mpfi_has_zero(x);
    
    // Gamma has a local minimum at psi_zero = 1.46163...< 1.5.
    // The local minimu has value Gamma(psi_zero) = 0.8856031944... < 1
    // Gamma(x) is positive for x > 0
    // On [0, psi_zero], Gamma is decreasing, on [psi_zero, Infinity] increasing
    // Gamma'/Gamma = digamma
    
    mpfi_interv_ui(ival, 1, 2);
  }
  else {
    mpz_t tmp_z;
    mpz_init(tmp_z);
    
    /* For integer n <= 0, Gamma is positive (negative) and convex (concave)
       on (n-1,n) if n is odd (even), with vertical asymptotes at n-1 and n.
     */
    
    mpfr_get_z(tmp_z, &x->left, MPFR_RNDU);
    if(mpfr_cmp_z(&x->right, tmp_z) >= 0) {
      if(mpfr_cmp_z(&x->left, tmp_z) < 0) { // x crosses a pole
        mpfr_set_inf(&result->left, -1);
        mpfr_set_inf(&result->right, 1);
        mpz_clear(tmp_z);
        mpfi_clear(ival);
        return 0;
      }
      else { // x.left = tmp_z is integer
        has_infinite_endpoint = TRUE;
        mpfr_set_z(&ival->left, tmp_z, MPFR_RNDD);
        mpfr_add_ui(&ival->right, &ival->left, 1, MPFR_RNDU);
        result_is_positive = mpz_even_p(tmp_z);
      }
    }
    else {
      has_infinite_endpoint = mpfr_integer_p(&x->right);
      mpfr_set_z(&ival->right, tmp_z, MPFR_RNDU);
      mpfr_sub_ui(&ival->left, &ival->right, 1, MPFR_RNDD);
      result_is_positive = mpz_odd_p(tmp_z);
    }
    
    mpz_clear(tmp_z);
  }
  
  /* The extrema of Gamma have an absolute value |Gamma(x*)| < 4
     Proof?
   */
  
  mpfi_init2(tmp,  10 + prec);
  mpfr_init2(val,  10 + prec);
  for(;;) {
    if(mpfr_lessequal_p(&x->right, &ival->left)) { // left of local extremum
      if(result_is_positive) { // left of minimum: decreasing
        // should go to result->left, but that might alias with x->left.
        mpfr_gamma(&result->right, &x->right, MPFR_RNDD);
        
        // should go to result->right, but that might alias with x->right.
        if(has_infinite_endpoint)
          mpfr_set_inf(&result->left, +1);
        else
          mpfr_gamma(&result->left, &x->left, MPFR_RNDU);
        
        mpfr_swap(&result->left, &result->right);
      }
      else { // left of maximum: increasing
        mpfr_gamma(&result->right, &x->right, MPFR_RNDU);
        if(has_infinite_endpoint)
          mpfr_set_inf(&result->left, -1);
        else
          mpfr_gamma(&result->left, &x->left, MPFR_RNDD);
      }
      
      mpfi_clear(ival);
      mpfi_clear(tmp);
      mpfr_clear(val);
      return 0;
    }
    
    if(mpfr_greaterequal_p(&x->left, &ival->right)) { // right of local extremum
      if(result_is_positive) { // right of minimum: increasing
        mpfr_gamma(&result->left, &x->left, MPFR_RNDD);
        if(has_infinite_endpoint)
          mpfr_set_inf(&result->left, +1);
        else
          mpfr_gamma(&result->right, &x->right, MPFR_RNDU);
      }
      else { // right of maximum: decreasing
        // should go to result->right, but that might alias with x->right.
        mpfr_gamma(&result->left, &x->left, MPFR_RNDU);
        
        // should go to result->left, but that might alias with x->left.
        if(has_infinite_endpoint)
          mpfr_set_inf(&result->right, -1);
        else
          mpfr_gamma(&result->right, &x->right, MPFR_RNDD);
        
        mpfr_swap(&result->left, &result->right);
      }
      
      mpfi_clear(ival);
      mpfi_clear(tmp);
      mpfr_clear(val);
      return 0;
    }
  
    //mpfi_diam_abs(val, ival);
    //if(mpfr_cmp_ui_2exp(val, 1, -prec) < 0) {
    //  if(result_is_positive) {
    //    if(has_infinite_endpoint) {
    //      mpfr_set_inf(&result->right, +1);
    //    }
    //    else {
    //      mpfr_gamma(&tmp->left,  &x->left,  MPFR_RNDU);
    //      mpfr_gamma(&tmp->right, &x->right, MPFR_RNDU);
    //      mpfr_max(&result->right, &tmp->left, &tmp->right, MPFR_RNDU);
    //    }
    //    
    //    mpfr_gamma(&tmp->left, &ival->right, MPFR_RNDD);
    //    mpfr_sub(&result->left, &tmp->left, val, MPFR_RNDD);
    //  }
    //  else {
    //    if(has_infinite_endpoint) {
    //      mpfr_set_inf(&result->left, -1);
    //    }
    //    else {
    //      mpfr_gamma(&tmp->left,  &x->left,  MPFR_RNDD);
    //      mpfr_gamma(&tmp->right, &x->right, MPFR_RNDD);
    //      mpfr_min(&result->left, &tmp->left, &tmp->right, MPFR_RNDD);
    //    }
    //    
    //    mpfr_gamma(&tmp->right, &ival->right, MPFR_RNDU);
    //    mpfr_add(&result->right, &tmp->right, val, MPFR_RNDU);
    //  }
    //  
    //  mpfi_clear(ival);
    //  mpfi_clear(tmp);
    //  mpfr_clear(val);
    //  return 0; // 3
    //}
  
    mpfi_bisect(ival, tmp, ival);
    mpfr_digamma(val, &ival->right, MPFR_RNDU);
    if(mpfr_sgn(val) <= 0) {
      mpfi_swap(ival, tmp);
      mpfr_abs(val, val, MPFR_RNDU);
    }
    
    mpfi_diam_abs(&tmp->right, ival);
    mpfr_mul(val, val, &tmp->right, MPFR_RNDU);
    mpfr_mul_ui(val, val, 4, MPFR_RNDU); /* val * sup |Gamma(x^*)| where Gamma'(x*) = 0  */
    if(mpfr_cmp_ui_2exp(val, 1, -prec) <= 0) {
      if(result_is_positive) {
        if(has_infinite_endpoint) {
          mpfr_set_inf(&result->right, +1);
        }
        else {
          mpfr_gamma(&tmp->left,  &x->left,  MPFR_RNDU);
          mpfr_gamma(&tmp->right, &x->right, MPFR_RNDU);
          mpfr_max(&result->right, &tmp->left, &tmp->right, MPFR_RNDU);
        }
        
        mpfr_gamma(&tmp->left, &ival->right, MPFR_RNDD);
        mpfr_sub(&result->left, &tmp->left, val, MPFR_RNDD);
      }
      else {
        if(has_infinite_endpoint) {
          mpfr_set_inf(&result->left, -1);
        }
        else {
          mpfr_gamma(&tmp->left,  &x->left,  MPFR_RNDD);
          mpfr_gamma(&tmp->right, &x->right, MPFR_RNDD);
          mpfr_min(&result->left, &tmp->left, &tmp->right, MPFR_RNDD);
        }
        
        mpfr_gamma(&tmp->right, &ival->right, MPFR_RNDU);
        mpfr_add(&result->right, &tmp->right, val, MPFR_RNDU);
      }
      
      mpfi_clear(ival);
      mpfi_clear(tmp);
      mpfr_clear(val);
      return 0; // 3
    }
    
    if(pmath_aborting())
      break;
  }
  
  mpfi_clear(tmp);
  mpfr_clear(val);
  mpfi_clear(ival);
  
  mpfr_set_inf(&result->left, -1);
  mpfr_set_inf(&result->right, 1);
  return 0;
}

PMATH_PRIVATE pmath_t builtin_gamma(pmath_expr_t expr) {
  pmath_t z;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  if(pmath_is_integer(z)) {
    if(pmath_is_int32(z)) { // Gamma(z) = (z-1)!
      unsigned long n;
      
      if(PMATH_AS_INT32(z) <= 0) {
        pmath_unref(expr);
        pmath_unref(z); // not neccessary
        return pmath_ref(_pmath_object_complex_infinity);
      }
      
      n = (unsigned long)(PMATH_AS_INT32(z) - 1);
      
      pmath_unref(z); // not neccessary
      z = factorial(n);
      if(pmath_is_null(z))
        return expr;
        
      pmath_unref(expr);
      return z;
    }
    
    if(pmath_number_sign(z) <= 0) {
      pmath_unref(expr);
      pmath_unref(z);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    
//    if(pmath_integer_fits_si32(z)){ // Gamma(z) = (z-1)!
//      unsigned long n = pmath_integer_get_ui(z) - 1;
//
//      pmath_unref(z);
//      z = factorial(n);
//      if(!pmath_is_null(z)){
//        pmath_unref(expr);
//        return z;
//      }
//    }

    pmath_unref(z);
    return expr;
  }
  
  if(pmath_is_quotient(z)) {
    pmath_integer_t den = pmath_rational_denominator(z);
    
    if(pmath_equals(den, PMATH_FROM_INT32(2))) {
      pmath_integer_t num = pmath_rational_numerator(z);
      
      if(pmath_is_int32(num)) {
        unsigned long unum = abs(PMATH_AS_INT32(num));
        
        if(PMATH_AS_INT32(num) > 0) {
          if(unum < 3) { // unum == 1
            pmath_unref(num); // not neccessary
            pmath_unref(den);
            pmath_unref(z);
            pmath_unref(expr);
            return SQRT(pmath_ref(PMATH_SYMBOL_PI));
          }
          
          // Gamma(n + 1/2) = (2n-1)!! / 2^n * Sqrt(Pi),   num = 2n+1
          pmath_unref(num); // not neccessary
          num = double_factorial(unum - 2);
          if(!pmath_is_null(num)) {
            long n = (signed long)(unum - 1) / 2;
            
            pmath_unref(den);
            pmath_unref(z);
            pmath_unref(expr);
            return TIMES3(num, POW(INT(2), INT(-n)), SQRT(pmath_ref(PMATH_SYMBOL_PI)));
          }
        }
        else {
          // Gamma(1/2 - n) = (-2)^n / (2n-1)!! Sqrt(Pi),  unum = -num = 2n-1
          pmath_unref(num); // not neccessary
          num = double_factorial(unum);
          if(!pmath_is_null(num)) {
            long n = (signed long)(unum + 1) / 2;
            
            pmath_unref(den);
            pmath_unref(z);
            pmath_unref(expr);
            return TIMES3(INV(num), POW(INT(-2), INT(n)), SQRT(pmath_ref(PMATH_SYMBOL_PI)));
          }
        }
      }
      
      pmath_unref(num);
    }
    
    pmath_unref(den);
  }
  
  if(pmath_is_double(z)) {
    pmath_unref(expr);
    return gamma_d(PMATH_AS_DOUBLE(z));
  }
  
  if(pmath_is_mpfloat(z)) {
    pmath_unref(expr);
    return _pmath_mpfloat_call(z, mpfr_gamma);
  }
  
  if(pmath_is_interval(z)) {
    pmath_unref(expr);
    return _pmath_interval_call(z, _mpfi_gamma);
  }
  
  { // infinite values
    int num_class = _pmath_number_class(z);
    
    if(num_class & PMATH_CLASS_POSINF) {
      pmath_unref(expr);
      return z;
    }
    
    if(num_class & PMATH_CLASS_NEGINF) {
      pmath_unref(z);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    }
    
    if(num_class & PMATH_CLASS_UINF) {
      pmath_unref(z);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    
    if( (num_class & PMATH_CLASS_CINF) &&
        (num_class & PMATH_CLASS_IMAGINARY))
    {
      pmath_unref(z);
      pmath_unref(expr);
      return INT(0);
    }
  }
  
  pmath_unref(z);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_loggamma(pmath_expr_t expr) {
  pmath_t z;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  if(pmath_is_integer(z)) {
    pmath_unref(expr);
    
    return LOG(GAMMA(z));
  }
  
  if(pmath_is_quotient(z)) {
    pmath_integer_t den = pmath_rational_denominator(z);
    
    if(pmath_equals(den, PMATH_FROM_INT32(2))) {
      pmath_unref(den);
      pmath_unref(expr);
      
      return LOG(GAMMA(z));
    }
    
    pmath_unref(den);
    pmath_unref(z);
    return expr;
  }
  
  { // infinite values
    int num_class = _pmath_number_class(z);
    
    if(num_class & PMATH_CLASS_POSINF) {
      pmath_unref(expr);
      return z;
    }
    
    if(num_class & PMATH_CLASS_NEGINF) {
      pmath_unref(z);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    }
    
    if(num_class & PMATH_CLASS_UINF) {
      pmath_unref(z);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    
    if( (num_class & PMATH_CLASS_CINF) &&
        (num_class & PMATH_CLASS_IMAGINARY))
    {
      pmath_unref(z);
      pmath_unref(expr);
      return INT(0);
    }
  }
  
  pmath_unref(z);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_polygamma(pmath_expr_t expr) {
// PolyGamma(n, z)
// PolyGamma(z)     = PolyGamma(0, z)
  size_t exprlen;
  unsigned long n;
  pmath_t z;
  
  exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  n = 0;
  if(exprlen == 2) {
    pmath_t n_obj = pmath_expr_get_item(expr, 1);
    
    if(!pmath_is_int32(n_obj) || PMATH_AS_INT32(n_obj) < 0) {
      pmath_unref(n_obj);
      return expr;
    }
    
    n = (unsigned)PMATH_AS_INT32(n_obj);
  }
  
  z = pmath_expr_get_item(expr, exprlen);
  if(n == 0) {
    if(pmath_is_int32(z)) {
      if(PMATH_AS_INT32(z) > 0) {
        unsigned long ui_z = (unsigned long)PMATH_AS_INT32(z);
        unsigned long k;
        pmath_unref(expr);
        pmath_unref(z); // not necessary
        
        z = INT(0);
        for(k = 1; k < ui_z && !pmath_aborting(); ++k) {
          z = _add_nn(z, QUOT(1, k));
        }
        
        return MINUS(z, pmath_ref(PMATH_SYMBOL_EULERGAMMA));
      }
      pmath_unref(z); // not necessary
      pmath_unref(expr);
      
      return CINFTY;
    }
    
    if(pmath_is_integer(z)) {
      if(pmath_number_sign(z) <= 0) {
        pmath_unref(z);
        pmath_unref(expr);
        
        return CINFTY;
      }
      
      pmath_unref(z);
      return expr;
    }
    
    if(pmath_is_quotient(z)) {
      pmath_t nn;
      
      if(pmath_number_sign(z) < 0) {
        pmath_unref(z);
        return expr;
      }
      
      nn = pmath_rational_denominator(z);
      if(pmath_equals(nn, PMATH_FROM_INT32(2))) {
        pmath_unref(nn);
        
        nn = pmath_rational_numerator(z);
        if(pmath_is_int32(nn) && PMATH_AS_INT32(nn) >= 0) {
          unsigned ui_num = (unsigned)PMATH_AS_INT32(nn);
          unsigned k;
          
          pmath_unref(expr);
          pmath_unref(z);
          pmath_unref(nn);
          
          // PolyGamma(n+1/2) = -EulerGamma - 2Log(2) + (2 + 2/3 + 2/5 + ... + 2/(2n-1))
          
          z = INT(0);
          for(k = 1; k < ui_num && !pmath_aborting(); k += 2) {
            z = _add_nn(z, QUOT(2, k));
          }
          
          return PLUS3(z, NEG(pmath_ref(PMATH_SYMBOL_EULERGAMMA)), TIMES(INT(-2), LOG(INT(2))));
        }
      }
      
      pmath_unref(nn);
      pmath_unref(z);
      return expr;
    }
  }
  
  { // infinite values
    int num_class = _pmath_number_class(z);
    
    if(num_class & PMATH_CLASS_POSINF) {
      pmath_unref(expr);
      return z;
    }
    
    if(num_class & PMATH_CLASS_UINF) {
      pmath_unref(expr);
      pmath_unref(z);
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    }
  }
  
  pmath_unref(z);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_factorial(pmath_expr_t expr) {
  pmath_t n;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  n = pmath_expr_get_item(expr, 1);
  if(pmath_is_int32(n)) {
    if(PMATH_AS_INT32(n) >= 0) {
      unsigned long un = (unsigned long)PMATH_AS_INT32(n);
      
      pmath_unref(n); // not necessary
      n = factorial(un);
      if(pmath_is_null(n))
        return expr;
        
      pmath_unref(expr);
      return n;
    }
    
    pmath_unref(n); // not necessary
    pmath_unref(expr);
    return pmath_ref(_pmath_object_complex_infinity);
  }
  
  if(pmath_is_integer(n)) {
    if(pmath_number_sign(n) < 0) {
      pmath_unref(n);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    
//    if(pmath_integer_fits_si32(n)){
//      unsigned long un = pmath_integer_get_ui(n);
//
//      pmath_unref(n);
//      n = factorial(un);
//      if(!pmath_is_null(n)){
//        pmath_unref(expr);
//        return n;
//      }
//    }

    pmath_unref(n);
    return expr;
  }
  
  if(pmath_is_quotient(n)) {
    pmath_integer_t den = pmath_rational_denominator(n);
    
    if(pmath_equals(den, PMATH_FROM_INT32(2))) {
      pmath_unref(den);
      goto AS_GAMMA;
    }
    
    pmath_unref(den);
    pmath_unref(n);
    return expr;
  }
  
  if(_pmath_is_inexact(n)) {
  AS_GAMMA:
    n = pmath_evaluate(GAMMA(PLUS(n, INT(1))));
    
    if(!pmath_is_expr_of(n, PMATH_SYMBOL_GAMMA)) {
      pmath_unref(expr);
      return n;
    }
    
    pmath_unref(n);
    return expr;
  }
  
  { // infinite values
    int num_class = _pmath_number_class(n);
    
    if(num_class & PMATH_CLASS_POSINF) {
      pmath_unref(expr);
      return n;
    }
    
    if(num_class & PMATH_CLASS_NEGINF) {
      pmath_unref(n);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    }
    
    if(num_class & PMATH_CLASS_UINF) {
      pmath_unref(n);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    
    if( (num_class & PMATH_CLASS_CINF) &&
        (num_class & PMATH_CLASS_IMAGINARY))
    {
      pmath_unref(n);
      pmath_unref(expr);
      return INT(0);
    }
  }
  
  pmath_unref(n);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_factorial2(pmath_expr_t expr) {
  pmath_t n;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  n = pmath_expr_get_item(expr, 1);
  if(pmath_is_int32(n)) {
    if(PMATH_AS_INT32(n) >= 0) {
      unsigned long un = (unsigned long)PMATH_AS_INT32(n);
      
      pmath_unref(n); // not necessary
      n = double_factorial(un);
      if(pmath_is_null(n))
        return expr;
        
      pmath_unref(expr);
      return n;
    }
    
    pmath_unref(n); // not necessary
    pmath_unref(expr);
    return pmath_ref(_pmath_object_complex_infinity);
  }
  
  if(pmath_is_integer(n)) {
    if(pmath_number_sign(n) < 0) {
      pmath_unref(n);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    
//    if(pmath_integer_fits_si32(n)){
//      unsigned long un = pmath_integer_get_ui(n);
//
//      pmath_unref(n);
//      n = double_factorial(un);
//      if(!pmath_is_null(n)){
//        pmath_unref(expr);
//        return n;
//      }
//    }

    pmath_unref(n);
    return expr;
  }
  
  if(pmath_is_quotient(n)) {
    pmath_integer_t den = pmath_rational_denominator(n);
    
    if(pmath_equals(den, PMATH_FROM_INT32(2))) {
      pmath_unref(den);
      goto AS_GAMMA;
    }
    
    pmath_unref(den);
    pmath_unref(n);
    return expr;
  }
  
  if(_pmath_is_inexact(n)) { // n!! = Sqrt(2^(n+1) / Pi) * Gamma(n/2 + 1)
    pmath_t res;
    
  AS_GAMMA:
    res = pmath_evaluate(
            TIMES(
              SQRT(
                DIV(
                  POW(
                    INT(2),
                    PLUS(pmath_ref(n), INT(1))),
                  pmath_ref(PMATH_SYMBOL_PI))),
              GAMMA(
                PLUS(
                  TIMES(ONE_HALF, pmath_ref(n)),
                  INT(1)))));
                  
    pmath_unref(n);
    if(!pmath_is_expr_of(res, PMATH_SYMBOL_GAMMA)) {
      pmath_unref(expr);
      return res;
    }
    
    pmath_unref(res);
    return expr;
  }
  
  { // infinite values
    int num_class = _pmath_number_class(n);
    
    if(num_class & PMATH_CLASS_POSINF) {
      pmath_unref(expr);
      return n;
    }
    
    if(num_class & PMATH_CLASS_NEGINF) {
      pmath_unref(n);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    }
    
    if(num_class & PMATH_CLASS_UINF) {
      pmath_unref(n);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    
    if( (num_class & PMATH_CLASS_CINF) &&
        (num_class & PMATH_CLASS_IMAGINARY))
    {
      pmath_unref(n);
      pmath_unref(expr);
      return INT(0);
    }
  }
  
  pmath_unref(n);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_binomial(pmath_expr_t expr) {
  pmath_t z, k;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  k = pmath_expr_get_item(expr, 2);
  
  if( (_pmath_is_inexact(z) &&  pmath_is_numeric(k)) ||
      ( pmath_is_numeric(z) && _pmath_is_inexact(k)))
  {
    /* Binomial(z, k) = z!/(k! (n-k)!) = Gamma(z+1) / (Gamma(k+1) * Gamma(z-k+1))
    
       Todo:
       if Re(z), Re(k) > 0:
                      = Exp(Log(Gamma(z+1) / (Gamma(k+1) * Gamma(z+1-k))))
                      = Exp(LogGamma(z+1) - LogGamma(k+1) - LogGamma(z+1-k))
     */
    pmath_unref(expr);
    expr = TIMES3(
             GAMMA(PLUS(pmath_ref(z), INT(1))),
             INV(GAMMA(PLUS(pmath_ref(k), INT(1)))),
             INV(GAMMA(PLUS3(pmath_ref(z), INT(1), NEG(pmath_ref(k))))));
             
    pmath_unref(z);
    pmath_unref(k);
    return expr;
  }
  
  if(pmath_is_int32(k)) {
    if(PMATH_AS_INT32(k) >= 0) {
      unsigned long uk = (unsigned long)PMATH_AS_INT32(k);
      
      switch(uk) {
        case 0:
          pmath_unref(expr);
          pmath_unref(z);
          pmath_unref(k); // not necessary
          return INT(1);
          
        case 1:
          pmath_unref(expr);
          pmath_unref(k); // not necessary
          return z;
          
        case 2:
          pmath_unref(expr);
          pmath_unref(k);
          expr = TIMES3(ONE_HALF, pmath_ref(z), PLUS(INT(-1), pmath_ref(z)));
          pmath_unref(z); // not necessary
          return expr;
      }
      
      if(pmath_is_int32(z)) {
        z = _pmath_create_mp_int(PMATH_AS_INT32(z));
        
        if(pmath_is_null(z)) {
          pmath_unref(expr);
          pmath_unref(k); // not necessary
          return z;
        }
      }
      
      if(pmath_is_mpint(z)) {
        pmath_integer_t result = _pmath_create_mp_int(0);
        
        if(!pmath_is_null(result)) {
          mpz_bin_ui(
            PMATH_AS_MPZ(result),
            PMATH_AS_MPZ(z),
            uk);
        }
        
        pmath_unref(expr);
        pmath_unref(z);
        pmath_unref(k); // not necessary
        return _pmath_mp_int_normalize(result);
      }
    }
    else { // Binomial(z, -k) = 0, if k is integer
      pmath_unref(z);
      pmath_unref(k); // not necessary
      pmath_unref(expr);
      return INT(0);
    }
  }
  
  if(pmath_is_integer(k)) {
    if(pmath_number_sign(k) < 0) { // Binomial(z, -k) = 0, if k is integer
      pmath_unref(z);
      pmath_unref(k);
      pmath_unref(expr);
      return INT(0);
    }
    
//    if(pmath_integer_fits_si32(k)){
//      unsigned long uk = pmath_integer_get_ui(k);
//
//      switch(uk){
//        case 0:
//          pmath_unref(expr);
//          pmath_unref(z);
//          pmath_unref(k);
//          return INT(1);
//
//        case 1:
//          pmath_unref(expr);
//          pmath_unref(k);
//          return z;
//
//        case 2:
//          pmath_unref(expr);
//          pmath_unref(k);
//          expr = TIMES3(ONE_HALF, pmath_ref(z), PLUS(INT(-1), pmath_ref(z)));
//          pmath_unref(z);
//          return expr;
//      }
//
//      if(pmath_is_integer(z)){
//        pmath_integer_t result = _pmath_create_mp_int();
//
//        if(!pmath_is_null(result)){
//          mpz_bin_ui(
//            PMATH_AS_MPZ(result),
//            PMATH_AS_MPZ(z),
//            uk);
//        }
//
//        pmath_unref(expr);
//        pmath_unref(z);
//        pmath_unref(k);
//        return result;
//      }
//    }
  }
  
  pmath_unref(z);
  pmath_unref(k);
  return expr;
}
