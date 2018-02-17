#include <pmath-core/numbers-private.h>

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
    
  if(n <= 166057019) { // 166057019!! >= 2^(2^31)
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

/** \brief Try to evaluate Gamma(z) of an infinite value z
    \param expr  Pointer to the Gamma-expression. On success, this will be replaced by the evaluation result.
    \param z     A pMath object. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_gamma_of_infinity(pmath_t *expr, pmath_t z) {
  pmath_t dir = _pmath_directed_infinity_direction(z);
  if(pmath_same(dir, PMATH_NULL))
    return FALSE;
  if(pmath_same(dir, INT(1))) {
    pmath_unref(dir);
    pmath_unref(*expr);
    *expr = pmath_ref(z);
    return TRUE;
  }
  if(pmath_same(dir, INT(-1))) {
    pmath_unref(dir);
    pmath_unref(*expr);
    *expr = pmath_ref(PMATH_SYMBOL_UNDEFINED);
    return TRUE;
  }
  if(pmath_same(dir, INT(0))) {
    pmath_unref(dir);
    pmath_unref(*expr);
    *expr = pmath_ref(_pmath_object_complex_infinity);
    return TRUE;
  }
  if(pmath_is_expr_of_len(dir, PMATH_SYMBOL_COMPLEX, 2)) {
    pmath_t re = pmath_expr_get_item(dir, 1);
    if(pmath_same(re, INT(0))) {
      pmath_unref(re);
      pmath_unref(dir);
      pmath_unref(*expr);
      *expr = pmath_ref(_pmath_object_complex_infinity);
      return TRUE;
    }
    pmath_unref(re);
  }
  pmath_unref(dir);
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_gamma(pmath_expr_t expr) {
  pmath_t z;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  if(pmath_complex_try_evaluate_acb(&expr, z, acb_gamma)) {
    pmath_unref(z);
    return expr;
  }
  
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

  if(_pmath_is_infinite(z)) {
    pmath_t dir = _pmath_directed_infinity_direction(z);
    if(pmath_same(dir, INT(1))) {
      pmath_unref(dir);
      pmath_unref(expr);
      return z;
    }
    if(pmath_same(dir, INT(-1))) {
      pmath_unref(dir);
      pmath_unref(z);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    }
    if(pmath_same(dir, INT(0))) {
      pmath_unref(dir);
      pmath_unref(z);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    if(pmath_is_expr_of_len(dir, PMATH_SYMBOL_COMPLEX, 2)) {
      pmath_t re = pmath_expr_get_item(dir, 1);
      if(pmath_same(re, INT(0))) {
        pmath_unref(re);
        pmath_unref(dir);
        pmath_unref(z);
        pmath_unref(expr);
        return INT(0);
      }
      pmath_unref(re);
    }
    pmath_unref(dir);
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
  if(pmath_complex_try_evaluate_acb(&expr, z, acb_lgamma)) {
    pmath_unref(z);
    return expr;
  }
  
  if(pmath_is_integer(z)) {
    pmath_unref(expr);
    
    if(pmath_number_sign(z) > 0) 
      return LOG(GAMMA(z));
    
    return pmath_ref(LOG(pmath_ref(_pmath_object_complex_infinity)));
  }
  
  if(pmath_is_quotient(z)) {
    pmath_integer_t den = pmath_rational_denominator(z);
    
    if(pmath_equals(den, PMATH_FROM_INT32(2))) {
      pmath_unref(den);
      pmath_unref(expr);
      
      if(pmath_number_sign(z) < 0) {
        /* LogGamm(x) = Log(Abs(Gamma(x))) + I * Pi * Floor(x) for x < 0
         */
        //pmath_t floor = pmath_expr_new_extended(
        //  pmath_ref(PMATH_SYMBOL_FLOOR), 1,
        //  pmath_ref(z));
        pmath_t floor = MINUS(pmath_ref(z), ONE_HALF); // = Floor(z)
        
        return PLUS(TIMES3(COMPLEX(INT(0), INT(1)), pmath_ref(PMATH_SYMBOL_PI), floor), LOG(ABS(GAMMA(z))));
      } 
      
      return LOG(GAMMA(z));
    }
    
    pmath_unref(den);
    pmath_unref(z);
    return expr;
  }
  
  if(try_gamma_of_infinity(&expr, z)) {
    pmath_unref(z);
    return expr;
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
  
  z = pmath_expr_get_item(expr, exprlen);
  n = 0;
  if(exprlen == 2) {
    pmath_t n_obj = pmath_expr_get_item(expr, 1);
    
    if(pmath_complex_try_evaluate_acb_2(&expr, n_obj, z, acb_polygamma)) {
      pmath_unref(n_obj);
      pmath_unref(z);
      return expr;
    }
    
    if(!pmath_is_int32(n_obj) || PMATH_AS_INT32(n_obj) < 0) {
      pmath_unref(n_obj);
      pmath_unref(z);
      return expr;
    }
    
    n = (unsigned)PMATH_AS_INT32(n_obj);
  }
  
  if(n == 0) {
    if(pmath_complex_try_evaluate_acb(&expr, z, acb_digamma)) {
      pmath_unref(z);
      return expr;
    }
    
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
  
  if(_pmath_is_infinite(z)) {
    pmath_t dir = _pmath_directed_infinity_direction(z);
    if(pmath_same(dir, INT(1))) {
      pmath_unref(dir);
      pmath_unref(expr);
      if(n >= 1) {
        pmath_unref(z);
        return INT(0);
      }
      return z;
    }
    if(pmath_same(dir, INT(0))) {
      pmath_unref(dir);
      pmath_unref(expr);
      pmath_unref(z);
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    }
    pmath_unref(dir);
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
  
  if(try_gamma_of_infinity(&expr, n)) {
    pmath_unref(n);
    return expr;
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
  
  if(_pmath_is_infinite(n)) {
    pmath_t dir = _pmath_directed_infinity_direction(n);
    if(pmath_same(dir, INT(1))) {
      pmath_unref(dir);
      pmath_unref(expr);
      return n;
    }
    if(pmath_same(dir, INT(0)) || pmath_same(dir, INT(-1))) {
      pmath_unref(dir);
      pmath_unref(expr);
      pmath_unref(n);
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    }
    if(pmath_is_expr_of_len(dir, PMATH_SYMBOL_COMPLEX, 2)) {
      pmath_t re = pmath_expr_get_item(dir, 1);
      if(pmath_same(re, INT(0))) {
        pmath_unref(re);
        pmath_unref(dir);
        pmath_unref(expr);
        pmath_unref(n);
        return pmath_ref(_pmath_object_complex_infinity);
      }
      pmath_unref(re);
    }
    pmath_unref(dir);
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
