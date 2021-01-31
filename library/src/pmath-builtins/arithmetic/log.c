#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>


extern pmath_symbol_t pmath_System_Complex;
extern pmath_symbol_t pmath_System_E;
extern pmath_symbol_t pmath_System_Log;
extern pmath_symbol_t pmath_System_Pi;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_Power;
extern pmath_symbol_t pmath_System_Times;
extern pmath_symbol_t pmath_System_Undefined;

static pmath_integer_t logii(pmath_integer_t base, pmath_integer_t z) {
  if(pmath_is_int32(base)) {
    base = _pmath_create_mp_int(PMATH_AS_INT32(base));
    
    z = logii(base, z);
    pmath_unref(base);
    return z;
  }
  
  if(pmath_is_int32(z)) {
    z = _pmath_create_mp_int(PMATH_AS_INT32(z));
    
    base = logii(base, z);
    pmath_unref(z);
    return base;
  }
  
  if( !pmath_is_null(base) &&
      !pmath_is_null(z) &&
      mpz_cmp_ui(PMATH_AS_MPZ(base), 1) >= 0)
  {
    pmath_mpint_t tmp = _pmath_create_mp_int(0);
    
    if(!pmath_is_null(tmp)) {
      mp_bitcnt_t result = mpz_remove(
                             PMATH_AS_MPZ(tmp),
                             PMATH_AS_MPZ(z),
                             PMATH_AS_MPZ(base));
                             
      if(mpz_cmp_ui(PMATH_AS_MPZ(tmp), 1) == 0) {
        pmath_unref(tmp);
        return pmath_integer_new_uiptr(result);
      }
      
      pmath_unref(tmp);
    }
  }
  
  return PMATH_NULL;
}

// assuming base, z > 0
// does not free base, z
static pmath_t logrr(pmath_rational_t base, pmath_rational_t z) {
  pmath_integer_t bn = pmath_rational_numerator(base);
  pmath_integer_t bd = pmath_rational_denominator(base);
  pmath_integer_t zn = pmath_rational_numerator(z);
  pmath_integer_t zd = pmath_rational_denominator(z);
  pmath_integer_t result;
  pmath_integer_t res2;
  int sign = 1;
  
  if(pmath_compare(bn, bd) < 1) {
    pmath_t tmp = bn;
    bn = bd;
    bd = tmp;
    
    sign = -sign;
  }
  
  if(pmath_compare(zn, zd) < 1) {
    pmath_t tmp = zn;
    zn = zd;
    zd = tmp;
    
    sign = -sign;
  }
  
  if(pmath_same(bd, INT(1))) {
    if(pmath_same(zd, INT(1))) {
      result = logii(bn, zn);
      
      if(!pmath_is_null(result)) {
        pmath_unref(bn);
        pmath_unref(bd);
        pmath_unref(zn);
        pmath_unref(zd);
        
        if(sign < 0)
          result = pmath_number_neg(result);
        return result;
      }
      
      pmath_unref(result);
    }
    
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    return PMATH_NULL;
  }
  
  result = logii(bn, zn);
  res2   = logii(bd, zd);
  if(pmath_equals(result, res2) && !pmath_is_null(result)) {
    pmath_unref(res2);
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    
    if(sign < 0)
      result = pmath_number_neg(result);
    return result;
  }
  
  pmath_unref(result);
  pmath_unref(res2);
  
  result = logii(bd, zn);
  res2   = logii(bn, zd);
  if(pmath_equals(result, res2)) {
    sign = -sign;
    pmath_unref(res2);
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    
    if(sign < 0)
      result = pmath_number_neg(result);
    return result;
  }
  
  pmath_unref(result);
  pmath_unref(res2);
  pmath_unref(bn);
  pmath_unref(bd);
  pmath_unref(zn);
  pmath_unref(zd);
  return PMATH_NULL;
}

static void _pmath_acb_log_with_base(acb_t res, const acb_t base, const acb_t x, slong prec) {
  acb_t log_base;
  acb_init(log_base);
  acb_log(log_base, base, prec);
  acb_log(res, x, prec);
  acb_div(res, res, log_base, prec);
  acb_clear(log_base);
}

/** \brief Try to evaluate Log(x) of an exact or inexact zero value x
    \param expr  Pointer to the Log-expression. On success, this will be replaced by the evaluation result.
    \param x     A pMath object. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_log_of_zero(pmath_t *expr, pmath_t x) {
  if(pmath_is_int32(x)) {
    if(!pmath_same(x, INT(0)))
      return FALSE;
      
    pmath_unref(*expr);
    *expr = pmath_ref(_pmath_object_neg_infinity);
    return TRUE;
  }
  if(pmath_is_float(x) || pmath_is_expr_of_len(x, pmath_System_Complex, 2)) {
    acb_t z;
    acb_init(z);
    if(_pmath_complex_float_extract_acb(z, NULL, NULL, x)) {
      if(acb_is_zero(z)) {
        pmath_unref(*expr);
        *expr = pmath_ref(_pmath_object_neg_infinity);
        acb_clear(z);
        return TRUE;
      }
      if(acb_contains_zero(z)) {
        pmath_unref(*expr);
        *expr = pmath_ref(pmath_System_Undefined);
        acb_clear(z);
        return TRUE;
      }
    }
    acb_clear(z);
  }
  return FALSE;
}

/** \brief Try to evaluate Log(x) of a non-zero rational number x
    \param expr  Pointer to the Log-expression. On success, this will be replaced by the evaluation result.
    \param x     A pMath object. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_log_of_nonzero_rational(pmath_t *expr, pmath_t x) {
  if(pmath_same(x, INT(1))) {
    pmath_unref(*expr);
    *expr = INT(0);
    return TRUE;
  }
  if(pmath_same(x, INT(-1))) {
    pmath_unref(*expr);
    *expr = TIMES(COMPLEX(INT(0), INT(1)), pmath_ref(pmath_System_Pi));
    return TRUE;
  }
  if(pmath_is_rational(x)) {
    int sign = pmath_number_sign(x);
    pmath_integer_t num, den;
    
    if(sign == 0)
      return FALSE;
      
    if(sign < 0) {
      pmath_unref(*expr);
      *expr = PLUS(TIMES(COMPLEX(INT(0), INT(1)), pmath_ref(pmath_System_Pi)), LOG(pmath_number_neg(pmath_ref(x))));
      return TRUE;
    }
    
    num = pmath_rational_numerator(x);
    den = pmath_rational_denominator(x);
    if(pmath_compare(num, den) < 0) {
      pmath_unref(num);
      pmath_unref(den);
      pmath_unref(*expr);
      *expr = NEG(LOG(pmath_rational_new(den, num)));
      return TRUE;
    }
    pmath_unref(num);
    pmath_unref(den);
  }
  return FALSE;
}

static pmath_bool_t try_log_of_nonreal_exact_complex(pmath_t *expr, pmath_t x) {
  pmath_t im = pmath_ref(x);
  if(_pmath_is_imaginary(&im) && pmath_is_rational(im)) {
    if(pmath_same(im, INT(1))) {
      pmath_unref(*expr);
      *expr = TIMES(COMPLEX(INT(0), ONE_HALF), pmath_ref(pmath_System_Pi));
      return TRUE;
    }
    if(pmath_same(im, INT(-1))) {
      pmath_unref(*expr);
      *expr = TIMES(COMPLEX(INT(0), QUOT(-1, 2)), pmath_ref(pmath_System_Pi));
      return TRUE;
    }
    if(pmath_number_sign(im) > 0) {
      pmath_unref(*expr);
      *expr = PLUS(TIMES(COMPLEX(INT(0), ONE_HALF), pmath_ref(pmath_System_Pi)), LOG(im));
      return TRUE;
    }
    if(pmath_number_sign(im) > 0) {
      pmath_unref(*expr);
      *expr = PLUS(TIMES(COMPLEX(INT(0), QUOT(-1, 2)), pmath_ref(pmath_System_Pi)), LOG(pmath_number_neg(im)));
      return TRUE;
    }
  }
  pmath_unref(im);
  return FALSE;
}

/** \brief Try to evaluate Log(x) of an infinite value x
    \param expr  Pointer to the Log-expression. On success, this will be replaced by the evaluation result.
    \param x     A pMath object. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_log_of_infinity(pmath_t *expr, pmath_t x) {
  pmath_t infdir = _pmath_directed_infinity_direction(x);
  if(pmath_same(infdir, PMATH_NULL))
    return FALSE;
    
  pmath_unref(infdir);
  pmath_unref(*expr);
  *expr = pmath_ref(_pmath_object_pos_infinity);
  return TRUE;
}

PMATH_PRIVATE pmath_t builtin_log(pmath_expr_t expr) {
  /* Log(base, x)
     Log(x)       = Log(E, x)
   */
  pmath_t base, x;
  
  if(pmath_expr_length(expr) == 1) {
    x = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_double(x) && PMATH_AS_DOUBLE(x) > 0) {
      pmath_unref(expr);
      return PMATH_FROM_DOUBLE(log(PMATH_AS_DOUBLE(x)));
    }
    
    base = pmath_ref(pmath_System_E);
  }
  else if(pmath_expr_length(expr) == 2) {
    base = pmath_expr_get_item(expr, 1);
    x    = pmath_expr_get_item(expr, 2);
  }
  else {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
  
  if(pmath_same(base, pmath_System_E) && pmath_complex_try_evaluate_acb(&expr, x, acb_log)) {
    pmath_unref(x);
    pmath_unref(base);
    return expr;
  }
  
  if(pmath_complex_try_evaluate_acb_2(&expr, base, x, _pmath_acb_log_with_base)) {
    pmath_unref(x);
    pmath_unref(base);
    return expr;
  }
  
  if( pmath_is_rational(base)     &&
      pmath_is_rational(x)        &&
      pmath_number_sign(base) > 0 &&
      pmath_number_sign(x)    > 0)
  {
    pmath_t result = logrr(base, x);
    
    if(!pmath_is_null(result)) {
      pmath_unref(expr);
      pmath_unref(base);
      pmath_unref(x);
      return result;
    }
  }
  
  if(pmath_equals(base, x) && !pmath_equals(base, PMATH_FROM_INT32(1))) {
    /* BUG: Log(0,0) should be Undefined instead of 1 */
    pmath_unref(expr);
    pmath_unref(base);
    pmath_unref(x);
    return INT(1);
  }
  
  if(pmath_is_expr_of_len(x, pmath_System_Power, 2)) {
    pmath_t b = pmath_expr_get_item(x, 1);
    
    if(pmath_equals(base, b)) {
      pmath_unref(b);
      b = pmath_expr_get_item(x, 2);
      
      if(pmath_is_numeric(b)) {
        pmath_unref(expr);
        pmath_unref(base);
        pmath_unref(x);
        return b;
      }
    }
    
    pmath_unref(b);
  }
  
  if(pmath_expr_length(expr) == 2) {
    pmath_unref(expr);
    return DIV(LOG(x), LOG(base));
  }
  pmath_unref(base); base = PMATH_NULL;
  
  if(_pmath_is_nonreal_complex_number(x)) {
    pmath_t re = pmath_expr_get_item(x, 1);
    pmath_t im = pmath_expr_get_item(x, 2);
    
    if(pmath_equals(re, PMATH_FROM_INT32(0))) {
      pmath_unref(x);
      pmath_unref(re);
      
      if(pmath_number_sign(im) < 0) {
        re = TIMES(
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_Complex), 2,
                 INT(0),
                 QUOT(-1, 2)),
               pmath_ref(pmath_System_Pi));
      }
      else {
        re = TIMES(
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_Complex), 2,
                 INT(0),
                 ONE_HALF),
               pmath_ref(pmath_System_Pi));
      }
      
      expr = pmath_expr_set_item(expr, 1, im);
      return PLUS(re, expr);
    }
  }
  
  if(try_log_of_zero(&expr, x))                  goto FINISH;
  if(try_log_of_nonzero_rational(&expr, x))      goto FINISH;
  if(try_log_of_nonreal_exact_complex(&expr, x)) goto FINISH;
  if(try_log_of_infinity(&expr, x))              goto FINISH;
  
FINISH:
  pmath_unref(x);
  return expr;
}
