#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>

static pmath_integer_t logii(pmath_integer_t base, pmath_integer_t z){
  struct _pmath_integer_t *bi = (struct _pmath_integer_t*)base;
  struct _pmath_integer_t *zi = (struct _pmath_integer_t*)z;
  
  if(bi && zi && mpz_cmp_ui(bi->value, 1) > 0){
    struct _pmath_integer_t *tmp = _pmath_create_integer();
    
    if(tmp){
      unsigned long result = mpz_remove(tmp->value, zi->value, bi->value);
      
      if(mpz_cmp_ui(tmp->value, 1) == 0){
        pmath_unref((pmath_integer_t)tmp);
        return pmath_integer_new_ui(result);
      }
      
      pmath_unref((pmath_integer_t)tmp);
    }
  }
  
  return NULL;
}

static pmath_integer_t logrr(pmath_rational_t base, pmath_rational_t z){
  pmath_integer_t bn = pmath_rational_numerator(base);
  pmath_integer_t bd = pmath_rational_denominator(base);
  pmath_integer_t zn = pmath_rational_numerator(z);
  pmath_integer_t zd = pmath_rational_denominator(z);
  pmath_integer_t result;
  pmath_integer_t res2;
  
  if(pmath_equals(bn, PMATH_NUMBER_ONE)){
    if(pmath_equals(zn, PMATH_NUMBER_ONE)){
      result = logii(bd, zd);
      
      if(result){
        pmath_unref(bn);
        pmath_unref(bd);
        pmath_unref(zn);
        pmath_unref(zd);
        return result;
      }
      
      pmath_unref(result);
    }
    
    if(pmath_equals(zd, PMATH_NUMBER_ONE)){
      result = logii(bd, zn);
      
      if(result){
        pmath_unref(bn);
        pmath_unref(bd);
        pmath_unref(zn);
        pmath_unref(zd);
        return pmath_number_neg(result);
      }
      
      pmath_unref(result);
    }
    
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    return NULL;
  }
  
  if(pmath_equals(bd, PMATH_NUMBER_ONE)){
    if(pmath_equals(zn, PMATH_NUMBER_ONE)){
      result = logii(bn, zd);
      
      if(result){
        pmath_unref(bn);
        pmath_unref(bd);
        pmath_unref(zn);
        pmath_unref(zd);
        return pmath_number_neg(result);
      }
      
      pmath_unref(result);
    }
    
    if(pmath_equals(zd, PMATH_NUMBER_ONE)){
      result = logii(bn, zn);
      
      if(result){
        pmath_unref(bn);
        pmath_unref(bd);
        pmath_unref(zn);
        pmath_unref(zd);
        return result;
      }
      
      pmath_unref(result);
    }
    
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    return NULL;
  }
  
  result = logii(bn, zn);
  res2   = logii(bd, zd);
  if(result && pmath_equals(result, res2)){
    pmath_unref(res2);
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    return result;
  }
  
  pmath_unref(result);
  pmath_unref(res2);
  
  result = logii(bd, zn);
  res2   = logii(bn, zd);
  if(pmath_equals(result, res2)){
    pmath_unref(res2);
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    return pmath_number_neg(result);
  }
  
  pmath_unref(result);
  pmath_unref(res2);
  pmath_unref(bn);
  pmath_unref(bd);
  pmath_unref(zn);
  pmath_unref(zd);
  return NULL;
}

PMATH_PRIVATE pmath_t builtin_log(pmath_expr_t expr){
/* Log(base, x)
   Log(x)       = Log(E, x)
 */
  pmath_t base, x;
  int xclass;
  
  if(pmath_expr_length(expr) == 1){
    base = pmath_ref(PMATH_SYMBOL_E);
    x    = pmath_expr_get_item(expr, 1);
  }
  else if(pmath_expr_length(expr) == 2){
    base = pmath_expr_get_item(expr, 1);
    x    = pmath_expr_get_item(expr, 2);
  }
  else{
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
  
  if(pmath_instance_of(base, PMATH_TYPE_RATIONAL) 
  && pmath_instance_of(x,    PMATH_TYPE_RATIONAL) 
  && pmath_number_sign(base) > 0
  && pmath_number_sign(x)    > 0){
    pmath_t result = logrr(base, x);
    
    if(result){
      pmath_unref(expr);
      pmath_unref(base);
      pmath_unref(x);
      return result;
    }
  }
  
  if(pmath_equals(base, x)
  && !pmath_equals(base, PMATH_NUMBER_ONE)){
    pmath_unref(expr);
    pmath_unref(base);
    pmath_unref(x);
    return INT(1);
  }
  
  if(pmath_is_expr_of_len(x, PMATH_SYMBOL_POWER, 2)){
    pmath_t b = pmath_expr_get_item(x, 1);
    
    if(pmath_equals(base, b)){
      pmath_unref(b);
      b = pmath_expr_get_item(x, 2);
      
      if(_pmath_is_numeric(b)){
        pmath_unref(expr);
        pmath_unref(base);
        pmath_unref(x);
        return b;
      }
    }
    
    pmath_unref(b);
  }
  
  if(pmath_expr_length(expr) == 2){
    pmath_unref(expr);
    return DIV(LOG(x), LOG(base));
  }
  pmath_unref(base); base = NULL;
  
  if(pmath_instance_of(x, PMATH_TYPE_MACHINE_FLOAT)
  && pmath_number_sign(x) > 0){
    pmath_unref(expr);
    expr = pmath_float_new_d(log(pmath_number_get_d(x)));
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_instance_of(x, PMATH_TYPE_MP_FLOAT)
  && pmath_number_sign(x) > 0){
    /* z = Log(x)
       error = dz = dx/x
       bits(z)  = -Log(2, dz/z) = -Log(2, dx/x/Log(x)) 
                = -Log(2, dx/x) + Log(2, Log(x))
                = bits(x) + Log(2, Log(x))
     */
    struct _pmath_mp_float_t *result;
    double dprec;
    
    result = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
    if(result){
      long exp;
      
      mpfr_log(
        result->value,
        ((struct _pmath_mp_float_t*)x)->value,
        GMP_RNDN);
      
      dprec = mpfr_get_d_2exp(&exp, result->value, GMP_RNDN);
      dprec = dprec + exp + pmath_precision(pmath_ref(x));
      
      if(dprec < 1)
        dprec = 1;
      else if(dprec > PMATH_MP_PREC_MAX)
        dprec = PMATH_MP_PREC_MAX;
        
      pmath_unref((pmath_float_t)result);
      result = _pmath_create_mp_float((mp_prec_t)ceil(dprec));
      if(result){
        mpfr_div(
          result->error, 
          ((struct _pmath_mp_float_t*)x)->error,
          ((struct _pmath_mp_float_t*)x)->value,
          GMP_RNDU);
        
        mpfr_log(
          result->value,
          ((struct _pmath_mp_float_t*)x)->value,
          GMP_RNDN);
        
        pmath_unref(expr);
        pmath_unref(x);
        return (pmath_float_t)result;
      }
    }
  }
  
  if(_pmath_is_nonreal_complex(x)){
    pmath_t re = pmath_expr_get_item(x, 1);
    pmath_t im = pmath_expr_get_item(x, 2);
    
    if(pmath_equals(re, PMATH_NUMBER_ZERO)){
      pmath_unref(x);
      pmath_unref(re);
      
      if(pmath_number_sign(im) < 0){
        re = TIMES(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
            INT(0),
            QUOT(-1, 2)),
          pmath_ref(PMATH_SYMBOL_PI));
      }
      else{
        re = TIMES(
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
            INT(0),
            ONE_HALF),
          pmath_ref(PMATH_SYMBOL_PI));
      }
      
      expr = pmath_expr_set_item(expr, 1, im);
      return PLUS(re, expr);
    }
  
    if(_pmath_is_inexact(re)
    || _pmath_is_inexact(im)){
      pmath_unref(re);
      pmath_unref(im);
      
      expr = pmath_expr_set_item(expr, 1, ABS(pmath_ref(x)));
      
      return pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
        expr,
        ARG(x));
    }
  }
  
  xclass = _pmath_number_class(x);
  if(xclass & PMATH_CLASS_INF){
    pmath_unref(expr);
    pmath_unref(x);
    return pmath_ref(_pmath_object_infinity);
  }
  
  if(xclass & PMATH_CLASS_ZERO){
    pmath_unref(expr);
    pmath_unref(x);
    return pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
      INT(-1));
  }
  
  if(xclass & PMATH_CLASS_POSSMALL){
    return NEG(pmath_expr_set_item(expr, 1, INV(x)));
  }
  
  if(xclass & PMATH_CLASS_NEG){
    expr = pmath_expr_set_item(expr, 1, NEG(x));
    return PLUS(
      expr, 
      TIMES(
        pmath_ref(PMATH_SYMBOL_I), 
        pmath_ref(PMATH_SYMBOL_PI)));
  }
  
  if(xclass == PMATH_CLASS_POSONE){
    pmath_unref(expr);
    pmath_unref(x);
    return INT(0);
  }
  
  pmath_unref(x);
  return expr;
}
