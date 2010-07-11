#include <assert.h>
#include <limits.h> // ULONG_MAX
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/numbers-private.h>

#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE 
pmath_bool_t _pmath_equals_rational(pmath_t obj, int n, int d){
  pmath_t part;
  
  if(!pmath_instance_of(obj, PMATH_TYPE_RATIONAL))
    return FALSE;
  
  part = pmath_rational_numerator(obj);
  if(!pmath_instance_of(part, PMATH_TYPE_INTEGER) 
  || !pmath_integer_fits_si(part)
  || n != pmath_integer_get_si(part)){
    pmath_unref(part);
    return FALSE;
  }
  
  pmath_unref(part);
  part = pmath_rational_denominator(obj);
  if(!pmath_instance_of(part, PMATH_TYPE_INTEGER) 
  || !pmath_integer_fits_si(part)
  || d != pmath_integer_get_si(part)){
    pmath_unref(part);
    return FALSE;
  }
  
  pmath_unref(part);
  return TRUE;
}

PMATH_PRIVATE 
pmath_bool_t _pmath_equals_rational_at(
  pmath_expr_t expr, 
  size_t i, 
  int n, int d
){
  pmath_t part = pmath_expr_get_item(expr, i);
  
  if(_pmath_equals_rational(part, n, d)){
    pmath_unref(part);
    return TRUE;
  }
  
  pmath_unref(part);
  return FALSE;
}

static pmath_integer_t _pow_i_abs(
  struct _pmath_integer_t  *base,    // wont be freed
  unsigned long             exponent
){ // TODO: prevent overflow / gmp-out-of-memory
  struct _pmath_integer_t *result;
  if(exponent == 1 || base == NULL)
    return (pmath_integer_t)pmath_ref((pmath_integer_t)base);

  result = _pmath_create_integer();
  if(!result)
    return NULL;

  mpz_pow_ui(
    result->value,
    base->value,
    exponent);
  return (pmath_integer_t)result;
}

/* a = int_root(&new_b, old_b, c)
   
   means old_b^(1/c) = a * new_b^(1/c)
   
   if a or new_b would be 1, NULL will be given.
   On Out-Of-Memory, both a and new_b may become NULL
 */
static pmath_integer_t int_root(
  pmath_integer_t           *new_base, 
  struct _pmath_integer_t   *old_base, // will be freed
  unsigned long              root_exp
){
  pmath_bool_t            neg;
  struct _pmath_integer_t   *iroot;
  struct _pmath_integer_t   *prime_power;
  unsigned int               i;
  
  assert(old_base != NULL);
  assert(new_base != NULL);
  
  if(mpz_cmpabs_ui(old_base->value, 1) == 0){
    if(mpz_sgn(old_base->value) > 0){
      pmath_unref((pmath_integer_t)old_base);
      *new_base = NULL;
      return NULL;
    }
      
    *new_base = (pmath_integer_t)old_base;
    return NULL;
  }
  
  neg = mpz_sgn(old_base->value) < 0;
  if(neg){
    old_base = (struct _pmath_integer_t*)pmath_number_neg((pmath_integer_t)old_base);
    
    if(!old_base){
      *new_base = NULL;
      return NULL;
    }
  }
  
  iroot = _pmath_create_integer();
  if(!iroot){
    pmath_unref((pmath_integer_t)old_base);
    *new_base = NULL;
    return NULL;
  }
  
  if(mpz_root(
      iroot->value,
      old_base->value,
      root_exp))
  {
    pmath_unref((pmath_integer_t)old_base);
    if(neg)
      *new_base = pmath_integer_new_si(-1);
    else
      *new_base = NULL;
    return (pmath_integer_t)iroot;
  }
  
  prime_power = _pmath_create_integer();
  if(!prime_power){
    pmath_unref((pmath_integer_t)iroot);
    pmath_unref((pmath_integer_t)old_base);
    *new_base = NULL;
    return NULL;
  }
  
  for(i = 0;i < (unsigned int)_pmath_primes16bit_count;++i){
    if(mpz_cmp_ui(iroot->value, i) < 0)
      break;
    
    if(mpz_divisible_ui_p(old_base->value, _pmath_primes16bit[i])){
      mpz_ui_pow_ui(
        prime_power->value,
        _pmath_primes16bit[i],
        root_exp);
      
      if(mpz_divisible_p(old_base->value, prime_power->value)){
        *new_base = (pmath_integer_t)_pmath_create_integer();
        if(!*new_base)
          break;
        
        mpz_divexact(
          (*(struct _pmath_integer_t **)new_base)->value,
          old_base->value,
          prime_power->value);
        
        pmath_unref((pmath_integer_t)iroot);
        pmath_unref((pmath_integer_t)prime_power);
        pmath_unref((pmath_integer_t)old_base);
        
        if(neg)
          *new_base = pmath_number_neg(*new_base);
          
        return pmath_integer_new_ui(_pmath_primes16bit[i]);
      }
    }
  }
  
  pmath_unref((pmath_integer_t)iroot);
  pmath_unref((pmath_integer_t)prime_power);
  
  if(neg)
    *new_base = pmath_number_neg((pmath_integer_t)old_base);
  else
    *new_base = (pmath_integer_t)old_base;
  
  return NULL;
}

static pmath_t _pow_ri(
  pmath_rational_t base,     // will be freed. not NULL!
  long             exponent
){
  pmath_integer_t num = pmath_rational_numerator(base);
  pmath_integer_t den = pmath_rational_denominator(base);
  
  assert(!pmath_equals(base, PMATH_NUMBER_ZERO));
  
  pmath_unref(base);
  if(exponent < 0){
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_POWER), 2, 
      pmath_rational_new(den, num),
      pmath_integer_new_si(-exponent));
  }
  
  base = num;
  num = _pow_i_abs(
    (struct _pmath_integer_t*)base, 
    (unsigned long)exponent);
  pmath_unref(base);
  
  base = den;
  den = _pow_i_abs(
    (struct _pmath_integer_t*)base, 
    (unsigned long)exponent);
  pmath_unref(base);
  
  if(pmath_equals(den, PMATH_NUMBER_ONE)){
    pmath_unref(den);
    return num;
  }
  
  // GCD(n, d) = 1  =>  GCD(n^e, d^e) = 1
  // => canonicalization not needed
  return (pmath_rational_t)_pmath_create_quotient(num, den);
}

static pmath_t _pow_fi(
  struct _pmath_mp_float_t *base,  // will be freed. not NULL!
  long                      exponent,
  pmath_bool_t              null_on_errors
){
  long lbaseexp;
  
  if(exponent <= 0 && mpfr_zero_p(base->value))    
    return (pmath_float_t)base;
    
  mpfr_get_d_2exp(&lbaseexp, base->value, GMP_RNDN);
  
  if(exponent * lbaseexp < MPFR_EMIN_DEFAULT){
    pmath_unref((pmath_t)base);
    if(null_on_errors)
      return NULL;
      
    pmath_message(PMATH_SYMBOL_GENERAL, "unfl", 0);
    return pmath_ref(_pmath_object_underflow);
  }
  
  if(exponent * lbaseexp > MPFR_EMAX_DEFAULT){
    pmath_unref((pmath_t)base);
    if(null_on_errors)
      return NULL;
    
    pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
    return pmath_ref(_pmath_object_overflow);
  }
  
  if(exponent < -1 || exponent > 1){
    struct _pmath_mp_float_t *err;
    struct _pmath_mp_float_t *result;
    
    // z = x^y,    dy = 0, x > 0, y > 1
    // error    = dz = x^(y-1) * y * dx       (dy = 0)
    // bits(z)  = -log(2, dz / z) = -log(2, y * dx / x) 
    //          = -log(2, y) - log(2, dx/x) = bits(x) - log(2, y)
    
    double dprec = ceil(pmath_precision(pmath_ref((pmath_t)base)) - log2(fabs(exponent)));
    
    if(dprec < 1)
      dprec = 1;
    else if(dprec > PMATH_MP_PREC_MAX)
      dprec = PMATH_MP_PREC_MAX; // ovfl/unfl error?
    
    result = _pmath_create_mp_float((mp_prec_t)ceil(dprec));
    err    = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
    if(!result || !err){
      pmath_unref((pmath_float_t)result);
      pmath_unref((pmath_float_t)err);
      pmath_unref((pmath_float_t)base);
      return NULL;
    }
    
    mpfr_abs(
      err->value, // x
      base->value,
      GMP_RNDU);
    
    mpfr_pow_si(
      err->error, // x^(y-1)
      err->value,
      exponent - 1,
      GMP_RNDU);
    
    mpfr_mul_ui(
      err->value, // x^(y-1) * y
      err->error,
      labs(exponent),
      GMP_RNDU);
    
    mpfr_mul(
      result->error, // x^(y-1) * y * dx = dz
      err->value,
      base->error,
      GMP_RNDU);
    
    
    mpfr_pow_si(
      result->value,
      base->value,
      exponent,
      GMP_RNDN);
    
    pmath_unref((pmath_float_t)err);
    pmath_unref((pmath_float_t)base);
    
    _pmath_mp_float_normalize(result);
    return (pmath_float_t)result;
  }

  if(exponent == -1){
    // z = 1/x,   x > 0
    // dz      = dx / x^2      (absoulte dz !!!, so no minus)
    // prec(z) = -log(2, dz/z) = -log(2, dx / x^2 / (1/x))
    //         = -log(2, dx/x) = prec(x)
    
    struct _pmath_mp_float_t *result;
    struct _pmath_mp_float_t *err;
    mp_prec_t prec = mpfr_get_prec(base->value);
    
    result = _pmath_create_mp_float(prec);
    err    = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
    
    if(result && err){
      mpfr_pow_si(
        err->error, // 1/x^2
        base->value,
        -2,
        GMP_RNDN);
      
      mpfr_abs(
        err->error,
        err->error,
        GMP_RNDN);
        
      mpfr_mul(
        result->error, // dx/x^2 = dz
        base->error,
        err->error,
        GMP_RNDU);
      
      mpfr_ui_div(
        result->value,
        1,
        ((struct _pmath_mp_float_t*)base)->value,
        GMP_RNDN);
    }
    
    pmath_unref((pmath_float_t)base);
    pmath_unref((pmath_float_t)err);
    return (pmath_float_t)result;
  }

  return (pmath_t)base;
}

static pmath_number_t _pow_ni_abs(
  pmath_number_t base, // will be freed
  unsigned long  exponent
){
  if(!base)
    return NULL;
  
  switch(base->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER: {
      pmath_integer_t result = _pow_i_abs((struct _pmath_integer_t*)base, exponent);
      pmath_unref(base);
      return result;
    }
    
    case PMATH_TYPE_SHIFT_QUOTIENT: {
      pmath_integer_t num = _pow_i_abs(((struct _pmath_quotient_t*)base)->numerator,   exponent);
      pmath_integer_t den = _pow_i_abs(((struct _pmath_quotient_t*)base)->denominator, exponent);
      
      pmath_unref(base);
      // GCD(n, d) = 1  =>  GCD(n^e, d^e) = 1
      // => canonicalization not needed
      return (pmath_rational_t)_pmath_create_quotient(num, den);
    }
    
    case PMATH_TYPE_SHIFT_MACHINE_FLOAT: {
      double d = pow(((struct _pmath_machine_float_t*)base)->value, exponent);
      
      if(isfinite(d) 
      && ((d == 0) == (((struct _pmath_machine_float_t*)base)->value == 0))){
        pmath_unref(base);
        return pmath_float_new_d(d);
      }
      
      base = (pmath_float_t)_pmath_convert_to_mp_float(base);
      if(!base)
        return NULL;
    }
    /* fall through */
    case PMATH_TYPE_SHIFT_MP_FLOAT: {
      return _pow_fi((struct _pmath_mp_float_t*)base, (long)exponent, TRUE);
    }
  }
  
  assert("not a number type" && 0);
  
  return base;
}

static pmath_number_t divide(
  pmath_number_t a, // will be freed
  pmath_number_t b  // will be freed
){
  if(!a || !b){
    pmath_unref(a);
    pmath_unref(b);
    return NULL;
  }
  
  switch(b->type_shift){
    case PMATH_TYPE_SHIFT_INTEGER: {
      if(mpz_cmp_ui(((struct _pmath_integer_t*)b)->value, 0) == 0){
        pmath_unref(b);
        return a;
      }
      
      return _mul_nn(a, (pmath_rational_t)_pmath_create_quotient(pmath_ref(PMATH_NUMBER_ONE), b));
    } break;
    
    case PMATH_TYPE_SHIFT_QUOTIENT: {
      pmath_integer_t num = pmath_rational_numerator(b);
      pmath_integer_t den = pmath_rational_denominator(b);
      
      pmath_unref(b);
      
      return _mul_nn(a, (pmath_rational_t)_pmath_create_quotient(den, num));
    } break;
    
    case PMATH_TYPE_SHIFT_MACHINE_FLOAT: {
      double y = ((struct _pmath_machine_float_t*)b)->value;
      
      if(y == 0){
        pmath_unref(b);
        return a;
      }
      
      y = 1/y;
      if(isfinite(y) && y != 0){
        pmath_unref(b);
        return _mul_nn(a, (pmath_float_t)_pmath_create_machine_float(y));
      }
      
      b = (pmath_float_t)_pmath_convert_to_mp_float(b);
      if(!b)
        return a;
    }
    /* fall through */
    case PMATH_TYPE_SHIFT_MP_FLOAT: {
      b = _pow_fi((struct _pmath_mp_float_t*)b, -1, TRUE);
      return _mul_nn(a, b);
    }
  }
  
  assert("not a number type" && 0);
  
  pmath_unref(a);
  pmath_unref(b);
  return NULL;
}

static void _pow_ci_abs(
  pmath_number_t *re_ptr,
  pmath_number_t *im_ptr,
  unsigned long   exponent
){
  struct _pmath_integer_t *bin;
  unsigned long k;
  pmath_number_t x, y, z;
  pmath_number_t dst[4];
  
  if(exponent == 1
  || !pmath_instance_of(*re_ptr, PMATH_TYPE_NUMBER)
  || !pmath_instance_of(*im_ptr, PMATH_TYPE_NUMBER)){
    return;
  }
  
  if(exponent >= LONG_MAX){
    pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
    pmath_unref(*re_ptr);
    pmath_unref(*im_ptr);
    *re_ptr = pmath_ref(_pmath_object_overflow);
    *im_ptr = pmath_ref(_pmath_object_overflow);
    return;
  }
  
  // (x+y)^n = Sum(Binomial(n, k) * x^(n-k) * y^k, k->0..n)
  // Binomial(n, k+1) = Binomial(n,k) * (n-k) / (k+1)
  
  x = *re_ptr;
  y = *im_ptr;
  
  dst[0] = pmath_integer_new_si(0);
  dst[1] = pmath_integer_new_si(0);
  dst[2] = pmath_integer_new_si(0);
  dst[3] = pmath_integer_new_si(0);
  
  z = _pow_ni_abs(pmath_ref(x), (long)exponent);
  bin = _pmath_create_integer();
  mpz_set_ui(bin->value, 1);
  
  for(k = 0;k < exponent;++k){
    dst[k & 3] = _add_nn(dst[k & 3], _mul_nn(pmath_ref((pmath_t)bin), pmath_ref(z)));
    
    mpz_mul_ui(     bin->value, bin->value, exponent - k);
    mpz_divexact_ui(bin->value, bin->value, k + 1);
    
    z = divide(_mul_nn(z, pmath_ref(y)), pmath_ref(x));
  }
  
  dst[exponent & 3] = _add_nn(dst[exponent & 3], _mul_nn((pmath_t)bin, z));
  
  pmath_unref(x);
  pmath_unref(y);
  
  *re_ptr = _add_nn(dst[0], pmath_number_neg(dst[2]));
  *im_ptr = _add_nn(dst[1], pmath_number_neg(dst[3]));
}

PMATH_PRIVATE pmath_t builtin_power(pmath_expr_t expr){
  pmath_t base;
  pmath_t exponent;
  int base_class;
  int exp_class;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  base =     pmath_expr_get_item(expr, 1);
  exponent = pmath_expr_get_item(expr, 2);
  
  if(pmath_instance_of(exponent, PMATH_TYPE_INTEGER)){
    if(!pmath_integer_fits_si(exponent)){
      if(pmath_instance_of(base, PMATH_TYPE_NUMBER)){
        pmath_unref(expr);
        
        if(pmath_instance_of(base, PMATH_TYPE_INTEGER)){
          if(pmath_integer_fits_si(base)){
            long si = pmath_integer_get_si(base);
            
            if(si == -1){
              if(mpz_odd_p(((struct _pmath_integer_t*)exponent)->value)){
                pmath_unref(exponent);
                return base;
              }
              
              pmath_unref(exponent);
              pmath_unref(base);
              return pmath_ref(PMATH_NUMBER_ONE);
            }
            
            if(si == 0 || si == 1){
              pmath_unref(exponent);
              return base;
            }
            
            pmath_unref(base);
            pmath_unref(exponent);
            pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
            return pmath_ref(_pmath_object_overflow);
          }
        }
        else if(pmath_instance_of(base, PMATH_TYPE_MACHINE_FLOAT)){
          double d = ((struct _pmath_machine_float_t*)base)->value;
          
          if(d == -1){
            if(mpz_odd_p(((struct _pmath_integer_t*)exponent)->value)){
              pmath_unref(exponent);
              return base;
            }
            
            pmath_unref(exponent);
            pmath_unref(base);
            return pmath_float_new_d(1.0);
          }
          
          if(d == 0 || d == 1){
            pmath_unref(exponent);
            return base;
          }
        }
        
        pmath_unref(base);
        if(pmath_number_sign(exponent) < 0){
          pmath_unref(exponent);
          pmath_message(PMATH_SYMBOL_GENERAL, "unfl", 0);
          return pmath_ref(_pmath_object_underflow);
        }
        
        pmath_unref(exponent);
        pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
        return pmath_ref(_pmath_object_overflow);
      }
      
      if(_pmath_is_nonreal_complex(base)){
        pmath_unref(expr);
        
        expr = pmath_expr_get_item(base, 1);
        if(pmath_instance_of(expr, PMATH_TYPE_NUMBER)
        && pmath_number_sign(expr) == 0){
          pmath_unref(expr);
          
          expr = pmath_expr_get_item(base, 2);
          if(pmath_instance_of(expr, PMATH_TYPE_INTEGER)){
            if(pmath_integer_fits_si(expr)){
              long si = pmath_integer_get_si(expr);
              
              if(si == 1 || si == -1){
                unsigned long ue = mpz_get_ui(((struct _pmath_integer_t*)exponent)->value);
                
                ue = ue & 3;
                if(ue != 0 && mpz_sgn(((struct _pmath_integer_t*)exponent)->value) < 0){
                  ue = 4 - ue;
                }
                
                pmath_unref(expr);
                pmath_unref(base);
                pmath_unref(exponent);
                switch(ue){
                  case 0: return INT(1);
                  case 1: return COMPLEX(INT(0), INT(si));
                  case 2: return INT(-1);
                  case 3: return COMPLEX(INT(0), INT(-si));
                }
                
                assert("unreachable code reached" && 0);
                return NULL;
              }
              
              pmath_unref(expr);
              pmath_unref(base);
              pmath_unref(exponent);
              pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
              return pmath_ref(_pmath_object_overflow);
            }
          }
          else if(pmath_instance_of(expr, PMATH_TYPE_MACHINE_FLOAT)){
            double d = ((struct _pmath_machine_float_t*)expr)->value;
            
            if(d == 1 || d == -1){
              unsigned long ue = mpz_get_ui(((struct _pmath_integer_t*)exponent)->value);
              
              ue = ue & 3;
              if(ue != 0 && mpz_sgn(((struct _pmath_integer_t*)exponent)->value) < 0){
                ue = 4 - ue;
              }
              
              pmath_unref(expr);
              pmath_unref(base);
              pmath_unref(exponent);
              switch(ue){
                case 0: return pmath_float_new_d(1.0);
                case 1: return COMPLEX(INT(0), pmath_float_new_d(d));
                case 2: return pmath_float_new_d(-1.0);
                case 3: return COMPLEX(INT(0), pmath_float_new_d(-d));
              }
              
              assert("unreachable code reached" && 0);
              return NULL;
            }
          }
        }
        
        pmath_unref(expr);
        expr = pmath_evaluate(FUNC(pmath_ref(PMATH_SYMBOL_ABS), base));
        
        if(pmath_instance_of(expr, PMATH_TYPE_NUMBER)
        && pmath_number_sign(expr) < 0){
          pmath_unref(expr);
          
          if(pmath_number_sign(exponent) < 0){
            pmath_unref(exponent);
            pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
            return pmath_ref(_pmath_object_overflow);
          }
          
          pmath_unref(exponent);
          pmath_message(PMATH_SYMBOL_GENERAL, "unfl", 0);
          return pmath_ref(_pmath_object_underflow);
        }
        else if(pmath_number_sign(exponent) < 0){
          pmath_unref(expr);
          pmath_unref(exponent);
          pmath_message(PMATH_SYMBOL_GENERAL, "unfl", 0);
          return pmath_ref(_pmath_object_underflow);
        }
        
        pmath_unref(expr);
        pmath_unref(exponent);
        pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
        return pmath_ref(_pmath_object_overflow);
      }
      
      pmath_unref(base);
      pmath_unref(exponent);
      return expr;
    }
    
    if(pmath_instance_of(base, PMATH_TYPE_MACHINE_FLOAT)
    && ((struct _pmath_machine_float_t*)base)->value != 0){
      double result = pow(
        ((struct _pmath_machine_float_t*)base)->value,
        pmath_integer_get_si(exponent));
      
      if(isfinite(result)){
        pmath_unref(expr);
        pmath_unref(exponent);
        
        if(base->refcount == 1){
          ((struct _pmath_machine_float_t*)base)->value = result;
          return base;
        }
        
        pmath_unref(base);
        return pmath_float_new_d(result);
      }
      
      base = (pmath_float_t)_pmath_convert_to_mp_float(base);
    }
    
    if(pmath_instance_of(base, PMATH_TYPE_MP_FLOAT)){
      long lexp = pmath_integer_get_si(exponent);
      if(lexp > 0 || !mpfr_zero_p(((struct _pmath_mp_float_t*)base)->value)){
        pmath_unref(exponent);
        pmath_unref(expr);
        return _pow_fi((struct _pmath_mp_float_t*)base, lexp, FALSE);
      }
    }
    
    if(pmath_instance_of(base, PMATH_TYPE_RATIONAL)
    && !pmath_equals(base, PMATH_NUMBER_ZERO)){
      long lexp = pmath_integer_get_si(exponent);
      pmath_unref(exponent);
      pmath_unref(expr);
      return _pow_ri(base, lexp);
    }
    
    if(pmath_is_expr_of_len(base, PMATH_SYMBOL_COMPLEX, 2)){
      pmath_t re = pmath_expr_get_item(base, 1);
      pmath_t im = pmath_expr_get_item(base, 2);
      long lexp = pmath_integer_get_si(exponent);
      
      if(pmath_instance_of(re, PMATH_TYPE_NUMBER)
      && pmath_instance_of(im, PMATH_TYPE_NUMBER)){
        if(pmath_equals(re, PMATH_NUMBER_ZERO)){
          // (I im)^n = I^n im^n
          int lexp4 = lexp % 4;
          
          if(lexp4 < 0)
            lexp4+= 4;
          
          pmath_unref(re);
          pmath_unref(base);
          pmath_unref(exponent);
          expr = pmath_expr_set_item(expr, 1, im);
          switch(lexp4){
            case 1: return COMPLEX(INT(0), expr);
            case 2: return NEG(expr);
            case 3: return COMPLEX(INT(0), NEG(expr));
          }
          
          return expr;
        }
        
        // (re + I im)^n
        if(lexp > 0){
          pmath_unref(expr);
          pmath_unref(exponent);
          _pow_ci_abs(&re, &im, (unsigned long)lexp);
          
          base = pmath_expr_set_item(base, 1, re);
          base = pmath_expr_set_item(base, 2, im);
          return base;
        }
        
        if(lexp < 0){ 
          pmath_unref(expr);
          pmath_unref(base);
          pmath_unref(exponent);
          
          _pow_ci_abs(&re, &im, (unsigned long)-lexp);
          
          base = pmath_evaluate(
            PLUS(
              TIMES(pmath_ref(re), pmath_ref(re)), 
              TIMES(pmath_ref(im), pmath_ref(im))));
          
          expr = COMPLEX(DIV(re, pmath_ref(base)), NEG(DIV(im, pmath_ref(base))));
          pmath_unref(base);
          return expr;
        }
      }
      
      pmath_unref(re);
      pmath_unref(im);
    }
    
    if(pmath_is_expr_of(base, PMATH_SYMBOL_TIMES)){
      size_t i;
      
      pmath_unref(expr);
      for(i = pmath_expr_length(base);i > 0;--i){
        base = pmath_expr_set_item(
          base, i,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_POWER), 2,
            pmath_expr_get_item(base, i),
            pmath_ref(exponent)));
      }
      
      pmath_unref(exponent);
      return base;
    }
  }
  
  if(pmath_instance_of(exponent, PMATH_TYPE_QUOTIENT)
  && pmath_instance_of(base, PMATH_TYPE_RATIONAL)
  && !pmath_equals(base, PMATH_NUMBER_ZERO)){ // (a/b)^(c/d) 
    pmath_integer_t exp_num;
    pmath_integer_t exp_den;
    
    if(pmath_number_sign(exponent) < 0){
      if(pmath_instance_of(base, PMATH_TYPE_INTEGER)){
        pmath_unref(base);
        pmath_unref(exponent);
        return expr;
      }
      
      pmath_unref(expr);
      
      expr = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_POWER), 2, 
        pmath_rational_new(
          pmath_rational_denominator(base), 
          pmath_rational_numerator(base)),
        pmath_number_neg(exponent));
      
      pmath_unref(base);
      return expr;
    }
    
    exp_num = pmath_rational_numerator(exponent);
    exp_den = pmath_rational_denominator(exponent);
    
    assert(exp_num != NULL);
    assert(exp_den != NULL);
    
    if(0 < mpz_cmpabs(
        ((struct _pmath_integer_t*)exp_num)->value, 
        ((struct _pmath_integer_t*)exp_den)->value))
    {
      struct _pmath_integer_t *qexp;
      struct _pmath_integer_t *rexp;
      pmath_unref(exponent);
      
      qexp = _pmath_create_integer();
      rexp = _pmath_create_integer();
      if(!qexp || !rexp){
        pmath_unref((pmath_integer_t)qexp);
        pmath_unref((pmath_integer_t)rexp);
        pmath_unref(exp_num);
        pmath_unref(exp_den);
        pmath_unref(base);
        pmath_unref(expr);
        return NULL;
      }
      
      mpz_tdiv_qr(
        qexp->value,
        rexp->value,
        ((struct _pmath_integer_t*)exp_num)->value,
        ((struct _pmath_integer_t*)exp_den)->value);
      
      pmath_unref(exp_num);
      
      expr = pmath_expr_set_item(
        expr, 2,
        (pmath_quotient_t)_pmath_create_quotient((pmath_integer_t)rexp, exp_den));
      
      return pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TIMES), 2,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_POWER), 2, 
          base,
          (pmath_integer_t)qexp),
        expr);
    }
    
    if(pmath_equals(exp_den, PMATH_NUMBER_TWO)
    && pmath_number_sign(base) < 0){
      // so exp_num = 1 or -1
      
      pmath_unref(expr);
      pmath_unref(exp_den);
      
      // 1/I = -I
      expr = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
        pmath_integer_new_si(0),
        pmath_integer_new_si(pmath_number_sign(exp_num)));
      
      pmath_unref(exp_num);
      base = pmath_number_neg(base);
      
      return pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TIMES), 2,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_POWER), 2,
          base,
          exponent),
        expr);
    }
    
    if(pmath_integer_fits_ui(exp_den)){
      pmath_integer_t base_num = pmath_rational_numerator(base);
      pmath_integer_t base_den = pmath_rational_denominator(base);
      pmath_integer_t base_num_root;
      pmath_integer_t base_den_root;
      unsigned long int root_exp = pmath_integer_get_ui(exp_den);
      
      pmath_unref(exp_den);
      exp_den = NULL;
      
      base_num_root = int_root(&base_num, (struct _pmath_integer_t*)base_num, root_exp);
      base_den_root = int_root(&base_den, (struct _pmath_integer_t*)base_den, root_exp);
      
      if(base_num_root || base_den_root){
        pmath_t result;
        
        if(!base_den_root)
          result = base_num_root;
        else if(!base_num_root)
          result = (pmath_t)_pmath_create_quotient(
            pmath_integer_new_si(1),
            base_den_root);
        else
          result = (pmath_t)_pmath_create_quotient(
            base_num_root, 
            base_den_root);
        
        pmath_unref(expr);
        
        // base_num_root, base_den_root, expr invalid now
        
        if(pmath_equals(exp_num, PMATH_NUMBER_ONE))
          pmath_unref(exp_num);
        else
          result = pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_POWER), 2,
            result,
            exp_num);
        
        // exp_num invalid now
        
        pmath_unref(base);
        if(base_num){
          if(base_den)
            base = (pmath_t)_pmath_create_quotient(base_num, base_den);
          else{
            base = base_num;
          }
        }
        else if(base_den){
          exponent = pmath_number_neg(exponent);
          base = base_den;
        }
        else
          base = NULL;
        
        // base_num, base_den invalid now
        if(base){
          result = pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_TIMES), 2, 
            result,
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_POWER), 2, 
              base,
              exponent));
        }
        else
          pmath_unref(exponent);
        
        return result;
      }
      
      pmath_unref(base_num);
      pmath_unref(base_den);
      pmath_unref(base_num_root);
      pmath_unref(base_den_root);
    }
    
    pmath_unref(exp_num);
    pmath_unref(exp_den);
  }
  
  if(pmath_instance_of(base, PMATH_TYPE_QUOTIENT)){
    pmath_integer_t num = pmath_rational_numerator(base);
    
    if(pmath_equals(num, PMATH_NUMBER_ONE)){
      pmath_unref(num);
      expr = pmath_expr_set_item(expr, 1, pmath_rational_denominator(base));
      pmath_unref(base);
      
      return pmath_expr_set_item(expr, 2, NEG(exponent));
    }
    
    pmath_unref(num);
  }
  
  if(_pmath_is_inexact(exponent)){
    if(pmath_equals(base, PMATH_SYMBOL_E)){
      if(pmath_instance_of(exponent, PMATH_TYPE_MP_FLOAT)){
        // dy = d(e^x) = e^x dx
        struct _pmath_mp_float_t *result;
        long prec;
        long exp;
        
        // Precision(y) = -Log(base, dy/y) = -Log(base, dx)
        mpfr_get_d_2exp(&exp, ((struct _pmath_mp_float_t*)exponent)->error, GMP_RNDU);
        prec = -exp;
        
        if(prec < MPFR_PREC_MIN)
          prec = MPFR_PREC_MIN;
        else if(prec > PMATH_MP_PREC_MAX)
          prec = PMATH_MP_PREC_MAX;
        
        result = _pmath_create_mp_float((mp_prec_t)prec);
        if(result){
          mpfr_exp(
            result->value,
            ((struct _pmath_mp_float_t*)exponent)->value,
            GMP_RNDN);
          
          mpfr_mul(
            result->error,
            result->value,
            ((struct _pmath_mp_float_t*)exponent)->error,
            GMP_RNDU);
          
          pmath_unref(exponent);
          pmath_unref(base);
          pmath_unref(expr);
          return (pmath_float_t)result;
        }
      }
    
      if(_pmath_is_nonreal_complex(exponent)){ // E^(x + I y) = E^x (Cos(y) + I Sin(y))
        pmath_t re = pmath_expr_get_item(exponent, 1);
        pmath_t im = pmath_expr_get_item(exponent, 2);
        
        expr = pmath_expr_set_item(expr, 2, re);
        
        expr = TIMES(expr, COMPLEX(COS(pmath_ref(im)), SIN(pmath_ref(im))));
        
        pmath_unref(im);
        pmath_unref(exponent);
        pmath_unref(base);
        return expr;
      }
    }
    
    if((pmath_instance_of(base,     PMATH_TYPE_NUMBER)
     && pmath_instance_of(exponent, PMATH_TYPE_MACHINE_FLOAT))
    || (pmath_instance_of(base,     PMATH_TYPE_MACHINE_FLOAT)
     && pmath_instance_of(exponent, PMATH_TYPE_NUMBER))){
      double b = pmath_number_get_d(base);
      double e = pmath_number_get_d(exponent);
      
      if(b < 0){
        double re = cos(M_PI * e);
        double im = sin(M_PI * e);
        double r = pow(-b, e);
        
        re*= r;
        im*= r;
        if(isfinite(re) && isfinite(im)){
          pmath_unref(expr);
          pmath_unref(base);
          pmath_unref(exponent);
          return pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_COMPLEX), 2, 
            pmath_float_new_d(re), 
            pmath_float_new_d(im));
        }
      }
      else{
        double result = pow(b, e);
        
        if(isfinite(result)){
          pmath_unref(expr);
          pmath_unref(base);
          pmath_unref(exponent);
          return pmath_float_new_d(result);
        }
      }
      
      expr = pmath_expr_set_item(expr, 1, NULL);
      base = pmath_set_precision(base, LOG10_2 * DBL_MANT_DIG);
      expr = pmath_expr_set_item(expr, 1, base);
      
      if(pmath_instance_of(exponent, PMATH_TYPE_MACHINE_FLOAT)){
        expr = pmath_expr_set_item(expr, 2, NULL);
        exponent = pmath_set_precision(exponent, LOG10_2 * DBL_MANT_DIG);
        expr = pmath_expr_set_item(expr, 2, exponent);
      }
      
      return expr;
    }
    
    if(pmath_instance_of(base,     PMATH_TYPE_MP_FLOAT)
    && pmath_instance_of(exponent, PMATH_TYPE_MP_FLOAT)){
      int basesign = mpfr_sgn(((struct _pmath_mp_float_t*)base)->value);
      
      if(basesign < 0 
      && !mpfr_integer_p(((struct _pmath_mp_float_t*)exponent)->value)){
      // (-x)^y = E^(y Log(-x)) = E^(y (I Pi + Log(x))) = x^y * E^(I Pi y)
      //        = x^y * (Cos(Pi y) + I Sin(Pi y))
        pmath_unref(expr);
        
        expr = COMPLEX(
          COS(TIMES(pmath_ref(exponent), pmath_ref(PMATH_SYMBOL_PI))),
          SIN(TIMES(pmath_ref(exponent), pmath_ref(PMATH_SYMBOL_PI))));
        
        if(mpfr_cmp_si(((struct _pmath_mp_float_t*)base)->value, -1) == 0){
          pmath_unref(base);
          pmath_unref(exponent);
          return expr;
        }
        
        return TIMES(POW(pmath_number_neg(base), exponent), expr);
      }
      
      if(basesign != 0){
      /* dz = d(x^y) = x^y * (y/x dx + Log(x) dy)
       */
        struct _pmath_mp_float_t *result;
        struct _pmath_mp_float_t *a;
        struct _pmath_mp_float_t *b;
        double dprec;
        long exp;
        
        a = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
        b = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
        
        if(a && b){
          // a->error = abs(x)
          mpfr_abs(
            a->error,
            ((struct _pmath_mp_float_t*)base)->value,
            GMP_RNDU);
          
          // b->error = log(abs(x))
          mpfr_log(
            b->error,
            a->error,
            GMP_RNDN);
          
          // a->error = log(abs(x)) * dy
          mpfr_mul(
            a->error,
            b->error,
            ((struct _pmath_mp_float_t*)exponent)->error,
            GMP_RNDU);
          
          // a->error = abs(log(abs(x)) * dy)
          mpfr_abs(
            a->error,
            a->error,
            GMP_RNDU);
          
          // b->value = y/x
          mpfr_div(
            b->value,
            ((struct _pmath_mp_float_t*)exponent)->value,
            ((struct _pmath_mp_float_t*)base)->value,
            GMP_RNDN);
          
          // b->error = y/x * dx
          mpfr_mul(
            b->error,
            b->value,
            ((struct _pmath_mp_float_t*)base)->error,
            GMP_RNDN);
          
          // b->error = abs(y/x * dx)
          mpfr_abs(
            b->error,
            b->error,
            GMP_RNDU);
          
          // b->value = abs(y/x * dx) + abs(log(abs(x)) * dy)
          mpfr_add(
            b->value,
            b->error,
            a->error,
            GMP_RNDU);
          
          // a->value = x^y
          mpfr_pow(
            a->value, 
            ((struct _pmath_mp_float_t*)base)->value,
            ((struct _pmath_mp_float_t*)exponent)->value,
            GMP_RNDN);
          
          // a->value = abs(x^y)
          mpfr_abs(
            a->value,
            a->value,
            GMP_RNDU);
          
          // precision: -log(2, dz/z)
          dprec = mpfr_get_d_2exp(&exp, b->value, GMP_RNDU);
          dprec = -dprec - exp;
          
          if(dprec < MPFR_PREC_MIN)
            dprec = MPFR_PREC_MIN;
          else if(dprec > PMATH_MP_PREC_MAX)
            dprec = PMATH_MP_PREC_MAX;
            
          result = _pmath_create_mp_float((mp_prec_t)ceil(dprec));
          if(result){
            mpfr_mul(
              result->error,
              a->value,
              b->value,
              GMP_RNDU);
            
            if(mpfr_zero_p(result->error)){
              pmath_unref((pmath_float_t)result);
              pmath_unref((pmath_float_t)a);
              pmath_unref((pmath_float_t)b);
              pmath_unref(exponent);
              pmath_unref(base);
              pmath_unref(expr);
              pmath_message(PMATH_SYMBOL_GENERAL, "unfl", 0);
              return pmath_ref(_pmath_object_underflow);
            }
            
            if(!mpfr_number_p(result->error)){
              pmath_unref((pmath_float_t)result);
              pmath_unref((pmath_float_t)a);
              pmath_unref((pmath_float_t)b);
              pmath_unref(exponent);
              pmath_unref(base);
              pmath_unref(expr);
              pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
              return pmath_ref(_pmath_object_overflow);
            }
            
            mpfr_pow(
              result->value, 
              ((struct _pmath_mp_float_t*)base)->value,
              ((struct _pmath_mp_float_t*)exponent)->value,
              GMP_RNDN);
            
            pmath_unref((pmath_float_t)a);
            pmath_unref((pmath_float_t)b);
            pmath_unref(exponent);
            pmath_unref(base);
            pmath_unref(expr);
            return (pmath_float_t)result;
          }
        }
        
        pmath_unref((pmath_float_t)a);
        pmath_unref((pmath_float_t)b);
      }
    }
    
    if(_pmath_is_nonreal_complex(exponent)
    || _pmath_is_nonreal_complex(base)){ 
      // x^y = Exp(y Log(x))
      
      expr = pmath_expr_set_item(expr, 1, pmath_ref(PMATH_SYMBOL_E));
      expr = pmath_expr_set_item(expr, 2, TIMES(exponent, LOG(base)));
      return expr;
    }
    
    if(!_pmath_is_inexact(base)){
      double prec = pmath_precision(exponent);
      
      expr = pmath_expr_set_item(expr, 1, NULL);
      base = pmath_approximate(base, prec, HUGE_VAL);
      expr = pmath_expr_set_item(expr, 1, base);
      return expr;
    }
  }
  else if(_pmath_is_inexact(base)){
    if(!_pmath_is_inexact(exponent)){
      double prec = pmath_precision(base);
      
      expr = pmath_expr_set_item(expr, 2, NULL);
      exponent = pmath_approximate(exponent, prec, HUGE_VAL);
      expr = pmath_expr_set_item(expr, 2, exponent);
      return expr;
    }
  }
  
  if(pmath_is_expr_of_len(base, PMATH_SYMBOL_POWER, 2)){ // (x^y)^exponent
    pmath_t inner_exp = pmath_expr_get_item(base, 2);
    
    if(pmath_instance_of(exponent, PMATH_TYPE_INTEGER)){
      pmath_unref(expr);
      return pmath_expr_set_item(base, 2, TIMES(exponent, inner_exp));
    }
    
    if(pmath_instance_of(exponent,  PMATH_TYPE_RATIONAL)
    && pmath_instance_of(inner_exp, PMATH_TYPE_RATIONAL)){
      if(pmath_number_sign(inner_exp) > 0){
        if(pmath_compare(inner_exp, PMATH_NUMBER_ONE) < 0){
          pmath_unref(expr);
          return pmath_expr_set_item(base, 2, TIMES(exponent, inner_exp));
        }
      }
      else{
        if(pmath_compare(inner_exp, PMATH_NUMBER_MINUSONE) > 0){
          pmath_unref(expr);
          return pmath_expr_set_item(base, 2, TIMES(exponent, inner_exp));
        }
      }
    }
    
    pmath_unref(inner_exp);
  }
  
  base_class = _pmath_number_class(base);
  exp_class  = _pmath_number_class(exponent);
  
  if(exp_class & (PMATH_CLASS_CINF | PMATH_CLASS_UINF)){
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_INDETERMINATE);
  }
  
  if(exp_class & PMATH_CLASS_POSINF){
    if(base_class & (PMATH_CLASS_ZERO | PMATH_CLASS_SMALL)){
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_integer_new_si(0);
    }
    
    if(base_class & (PMATH_CLASS_INF | PMATH_CLASS_NEGBIG)){
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    
    if(base_class & (PMATH_CLASS_POSBIG)){
      pmath_unref(base);
      pmath_unref(expr);
      return exponent;
    }
    
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_INDETERMINATE);
  }
  
  if(exp_class & PMATH_CLASS_ZERO){
    if(base_class & (PMATH_CLASS_ZERO | PMATH_CLASS_INF)){
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_message(NULL, "indet", 1, expr);
      return pmath_ref(PMATH_SYMBOL_INDETERMINATE);
    }
    
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_unref(expr);
    return pmath_integer_new_si(1);
  }
  
  if(base_class & PMATH_CLASS_ZERO){
    if(exp_class & PMATH_CLASS_NEG){
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_message(NULL, "infy", 1, expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    
    if(exp_class & PMATH_CLASS_POS){
      pmath_unref(exponent);
      pmath_unref(expr);
      return base;
    }
    
    if(exp_class & PMATH_CLASS_UNKNOWN){
      pmath_unref(base);
      pmath_unref(exponent);
      return expr;
    }
    
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_INDETERMINATE);
  }
  
  if(base_class & (PMATH_CLASS_NEGINF | PMATH_CLASS_CINF)){
    if(exp_class & PMATH_CLASS_NEG){
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_integer_new_si(0);
    }
    
    if(exp_class & PMATH_CLASS_POSINF){
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    
    if(exp_class & PMATH_CLASS_POS){
      expr = pmath_expr_set_item(
        expr, 1, 
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_SIGN), 1,
          base));
      return pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1, expr);
    }
    
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_INDETERMINATE);
  }
  
  if(base_class & PMATH_CLASS_POSINF){
    if(exp_class & PMATH_CLASS_NEG){
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_integer_new_si(0);
    }
    
    if(exp_class & PMATH_CLASS_POS){
      pmath_unref(exponent);
      pmath_unref(expr);
      return base;
    }
    
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_INDETERMINATE);
  }
  
  if(exp_class & PMATH_CLASS_NEGINF){
    if(base_class & (PMATH_CLASS_BIG | PMATH_CLASS_INF)){
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_integer_new_si(0);
    }
    
    if(base_class & PMATH_CLASS_NEGSMALL){
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }
    
    if(base_class & PMATH_CLASS_POSSMALL){
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_infinity);
    }
    
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_INDETERMINATE);
  }
  
  if(base_class & PMATH_CLASS_POSONE){
    pmath_unref(exponent);
    pmath_unref(expr);
    return base;
  }
  
  if(base_class & PMATH_CLASS_UINF){
    if(exp_class & PMATH_CLASS_NEG){
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_integer_new_si(0);
    }
    
    if(exp_class & PMATH_CLASS_POS){
      pmath_unref(exponent);
      pmath_unref(expr);
      return base;
    }
    
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_INDETERMINATE);
  }
  
  if(exp_class & PMATH_CLASS_POSONE){
    pmath_unref(exponent);
    pmath_unref(expr);
    return base;
  }
  
  if(pmath_equals(exponent, _pmath_object_overflow)
  || pmath_equals(exponent, _pmath_object_underflow)
  || pmath_equals(base, _pmath_object_overflow)
  || pmath_equals(base, _pmath_object_underflow)){
    pmath_unref(expr);
    
    if(pmath_instance_of(exponent, PMATH_TYPE_RATIONAL)){
      if(pmath_number_sign(exponent) < 0){
        pmath_unref(exponent);
        if(pmath_equals(base, _pmath_object_overflow)){
          pmath_unref(base);
          return pmath_ref(_pmath_object_underflow);
        }
        pmath_unref(base);
        return pmath_ref(_pmath_object_overflow);
      }
      
      pmath_unref(exponent);
      return base;
    }
    
    pmath_unref(base); 
    pmath_unref(exponent); 
    return pmath_ref(PMATH_SYMBOL_INDETERMINATE); 
  }
  
  pmath_unref(base);
  pmath_unref(exponent);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_exp(pmath_expr_t expr){
  pmath_t x;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  x = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_POWER), 2,
    pmath_ref(PMATH_SYMBOL_E),
    x);
}

PMATH_PRIVATE pmath_t builtin_sqrt(pmath_expr_t expr){
  pmath_t x;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  x = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_POWER), 2,
    x,
    pmath_ref(_pmath_one_half));
}

PMATH_PRIVATE pmath_t builtin_approximate_power(
  pmath_t obj, 
  double prec, 
  double acc
){
  pmath_t base, exp;
  
  if(!pmath_is_expr_of_len(obj, PMATH_SYMBOL_POWER, 2))
    return obj;
  
  base = pmath_expr_get_item(obj, 1);
  exp  = pmath_expr_get_item(obj, 2);
  
  if(pmath_instance_of(exp, PMATH_TYPE_INTEGER)){
    pmath_unref(exp);
    base = _pmath_approximate_step(base, prec, acc);
    return pmath_expr_set_item(obj, 1, base);
  }
  
  base = _pmath_approximate_step(base, prec, acc);
  exp  = _pmath_approximate_step(exp, prec, acc);
  obj = pmath_expr_set_item(obj, 1, base);
  obj = pmath_expr_set_item(obj, 2, exp);
  return obj;
}
