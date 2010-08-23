#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-core/numbers-private.h>

#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

static pmath_integer_t factorial(unsigned long n){
  if(n == 0)
    return pmath_integer_new_si(1);

  if(n <= 86180310){ // 86180310! >= 2^31
    struct _pmath_integer_t *result = _pmath_create_integer();

    if(result){
      mpz_set_ui(result->value, n);

      while(--n > 0 && !pmath_aborting()){
        mpz_mul_ui(result->value, result->value, n);
      }

      return (pmath_t)result;
    }
  }

  return NULL;
}

static pmath_integer_t double_factorial(unsigned long n){
  if(n <= 1)
    return pmath_integer_new_si(1);

  if(n <= 166057019){ // 166057019!! >= 2^31
    struct _pmath_integer_t *result = _pmath_create_integer();

    if(result){
      mpz_set_ui(result->value, n);

      while(n > 2 && !pmath_aborting()){
        n-= 2;
        mpz_mul_ui(result->value, result->value, n);
      }

      return (pmath_t)result;
    }
  }

  return NULL;
}



PMATH_PRIVATE pmath_t builtin_gamma(pmath_expr_t expr){
  pmath_t z;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  z = pmath_expr_get_item(expr, 1);
  if(pmath_instance_of(z, PMATH_TYPE_INTEGER)){
    if(pmath_number_sign(z) <= 0){
      pmath_unref(expr);
      pmath_unref(z);
      return pmath_ref(_pmath_object_complex_infinity);
    }

    if(pmath_integer_fits_si(z)){ // Gamma(z) = (z-1)!
      unsigned long n = pmath_integer_get_ui(z) - 1;

      pmath_unref(z);
      z = factorial(n);
      if(z){
        pmath_unref(expr);
        return z;
      }
    }

    pmath_unref(z);
    return expr;
  }

  if(pmath_instance_of(z, PMATH_TYPE_QUOTIENT)){
    pmath_integer_t den = pmath_rational_denominator(z);

    if(pmath_equals(den, PMATH_NUMBER_TWO)){
      pmath_integer_t num = pmath_rational_numerator(z);

      if(pmath_integer_fits_si(num)){
        unsigned long unum = pmath_integer_get_ui(num);

        if(pmath_number_sign(num) > 0){
          if(unum < 3){ // unum == 1
            pmath_unref(num);
            pmath_unref(den);
            pmath_unref(z);
            pmath_unref(expr);
            return SQRT(pmath_ref(PMATH_SYMBOL_PI));
          }

          // Gamma(n + 1/2) = (2n-1)!! / 2^n * Sqrt(Pi),   num = 2n+1
          pmath_unref(num);
          num = double_factorial(unum - 2);
          if(num){
            long n = (signed long) (unum-1)/2;

            pmath_unref(den);
            pmath_unref(z);
            pmath_unref(expr);
            return TIMES3(num, POW(INT(2), INT(-n)), SQRT(pmath_ref(PMATH_SYMBOL_PI)));
          }
        }
        else{
          // Gamma(1/2 - n) = (-2)^n / (2n-1)!! Sqrt(Pi),  unum = -num = 2n-1
          pmath_unref(num);
          num = double_factorial(unum);
          if(num){
            long n = (signed long) (unum+1)/2;

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

//  if(pmath_instance_of(z, PMATH_TYPE_MACHINE_FLOAT)){
//    double d = ((struct _pmath_machine_float_t*)z)->value;
//    struct _pmath_mp_float_t *result;
//
//    result = _pmath_create_mp_float_from_d(d);
//    if(result){
//      mpfr_gamma(result->value, result->value, GMP_RNDN);
//
//      if(mpfr_nan_p(result->value)){
//        pmath_unref((pmath_t)result);
//        pmath_unref(z);
//        pmath_unref(expr);
//        return pmath_ref(_pmath_object_complex_infinity);
//      }
//
//      if(!mpfr_number_p(result->value)){
//        pmath_unref((pmath_t)result);
//        pmath_unref(z);
//        pmath_unref(expr);
//        pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
//        return pmath_ref(_pmath_object_overflow);
//      }
//
//      d = mpfr_get_d(result->value, GMP_RNDN);
//      if(isfinite(d)){
//        pmath_unref((pmath_t)result);
//        pmath_unref(z);
//        pmath_unref(expr);
//        return pmath_float_new_d(d);
//      }
//
//      pmath_unref(z);
//      pmath_unref(expr);
//      return (pmath_t)result;
//    }
//  }
//
//  if(pmath_instance_of(z, PMATH_TYPE_MP_FLOAT)){
//
//  }




//  if(_pmath_is_infinite(z)){
//    pmath_t dir = _pmath_directed_infinity_direction(z);
//
//
//  }

  pmath_unref(z);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_loggamma(pmath_expr_t expr){
  pmath_t z;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  z = pmath_expr_get_item(expr, 1);
  // ...

  pmath_unref(z);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_factorial(pmath_expr_t expr){
  pmath_t n;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  n = pmath_expr_get_item(expr, 1);
  if(pmath_instance_of(n, PMATH_TYPE_INTEGER)){
    if(pmath_number_sign(n) < 0){
      pmath_unref(n);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }

    if(pmath_integer_fits_si(n)){
      unsigned long un = pmath_integer_get_ui(n);

      pmath_unref(n);
      n = factorial(un);
      if(n){
        pmath_unref(expr);
        return n;
      }
    }

    pmath_unref(n);
    return expr;
  }

  if(pmath_instance_of(n, PMATH_TYPE_QUOTIENT)){
    pmath_integer_t den = pmath_rational_denominator(n);

    if(pmath_equals(den, PMATH_NUMBER_TWO)){
      pmath_unref(den);
      goto AS_GAMMA;
    }

    pmath_unref(den);
    pmath_unref(n);
    return expr;
  }

  if(_pmath_is_inexact(n)){ AS_GAMMA:
    n = pmath_evaluate(GAMMA(PLUS(n, INT(1))));

    if(!pmath_is_expr_of(n, PMATH_SYMBOL_GAMMA)){
      pmath_unref(expr);
      return n;
    }

    pmath_unref(n);
    return expr;
  }

  pmath_unref(n);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_factorial2(pmath_expr_t expr){
  pmath_t n;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  n = pmath_expr_get_item(expr, 1);
  if(pmath_instance_of(n, PMATH_TYPE_INTEGER)){
    if(pmath_number_sign(n) < 0){
      pmath_unref(n);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }

    if(pmath_integer_fits_si(n)){
      unsigned long un = pmath_integer_get_ui(n);

      pmath_unref(n);
      n = double_factorial(un);
      if(n){
        pmath_unref(expr);
        return n;
      }
    }

    pmath_unref(n);
    return expr;
  }
  if(pmath_instance_of(n, PMATH_TYPE_QUOTIENT)){
    pmath_integer_t den = pmath_rational_denominator(n);

    if(pmath_equals(den, PMATH_NUMBER_TWO)){
      pmath_unref(den);
      goto AS_GAMMA;
    }

    pmath_unref(den);
    pmath_unref(n);
    return expr;
  }

  if(_pmath_is_inexact(n)){ // n!! = Sqrt(2^(n+1) / Pi) * Gamma(n/2 + 1)
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
    if(!pmath_is_expr_of(res, PMATH_SYMBOL_GAMMA)){
      pmath_unref(expr);
      return res;
    }

    pmath_unref(res);
    return expr;
  }

  pmath_unref(n);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_binomial(pmath_expr_t expr){
  pmath_t z, k;

  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  z = pmath_expr_get_item(expr, 1);
  k = pmath_expr_get_item(expr, 2);

  if((_pmath_is_inexact(z) && _pmath_is_numeric(k))
  || (_pmath_is_numeric(z) && _pmath_is_inexact(k))){
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

  if(pmath_instance_of(k, PMATH_TYPE_INTEGER)){
    if(pmath_number_sign(k) < 0){ // Binomial(z, -k) = 0, if k is integer
      pmath_unref(z);
      pmath_unref(k);
      pmath_unref(expr);
      return INT(0);
    }

    if(pmath_integer_fits_si(k)){
      unsigned long uk = pmath_integer_get_ui(k);

      switch(uk){
        case 0:
          pmath_unref(expr);
          pmath_unref(z);
          pmath_unref(k);
          return INT(1);

        case 1:
          pmath_unref(expr);
          pmath_unref(k);
          return z;

        case 2:
          pmath_unref(expr);
          pmath_unref(k);
          expr = TIMES3(ONE_HALF, pmath_ref(z), PLUS(INT(-1), pmath_ref(z)));
          pmath_unref(z);
          return expr;
      }

      if(pmath_instance_of(z, PMATH_TYPE_INTEGER)){
        struct _pmath_integer_t *result = _pmath_create_integer();

        if(result){
          mpz_bin_ui(
            result->value,
            ((struct _pmath_integer_t*)z)->value,
            uk);
        }

        pmath_unref(expr);
        pmath_unref(z);
        pmath_unref(k);
        return (pmath_t)result;
      }
    }
  }

  pmath_unref(z);
  pmath_unref(k);
  return expr;
}
