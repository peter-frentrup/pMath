#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/language-private.h>
#include <pmath-builtins/number-theory-private.h>

#include <limits.h>


/** Assumes that for expr == f(...), the following is true:
      f(f(x)) = f(x)
      f(x * y) = f(x) * f(y)
    and tries to simplify expr uppon these assumptions.

    example: abs(-4 a Pi abs(b)) ---> 4 Pi abs(a b)

    Frees expr.
 */
static pmath_t simplify_abs_sign(pmath_expr_t expr) {
  pmath_t x;
  pmath_t head, xhead;
  size_t xlen;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(x)) {
    pmath_unref(x);
    return expr;
  }
  
  head = pmath_expr_get_item(expr, 0);
  xhead = pmath_expr_get_item(x, 0);
  xlen = pmath_expr_length(x);
  
  if(xlen == 1 && pmath_equals(xhead, head)) {
    pmath_unref(xhead);
    pmath_unref(head);
    pmath_unref(expr);
    return x;
  }
  
  if(pmath_same(xhead, PMATH_SYMBOL_TIMES)) {
    pmath_expr_t extracted = pmath_expr_new(pmath_ref(PMATH_SYMBOL_TIMES), xlen);
    size_t ei = 0;
    size_t i;
    
    for(i = 1; i <= xlen; i++) {
      pmath_expr_t xi = pmath_expr_get_item(x, i);
      pmath_bool_t do_extract;
      
      if(pmath_is_expr(xi)
          && pmath_expr_length(xi) == 1) {
        pmath_t xi_head = pmath_expr_get_item(xi, 0);
        
        if(pmath_equals(head, xi_head)) {
          pmath_unref(xi_head);
          
          x = pmath_expr_set_item(x, i,
                                  pmath_expr_get_item(xi, 1));
          pmath_unref(xi);
          
          continue;
        }
        pmath_unref(xi_head);
      }
      
      xi = pmath_evaluate(FUNC(pmath_ref(head), xi));
      
      do_extract = !pmath_is_expr(xi) || pmath_expr_length(xi) != 1;
      
      if(!do_extract) {
        pmath_t xi_head = pmath_expr_get_item(xi, 0);
        do_extract = !pmath_equals(head, xi_head);
        pmath_unref(xi_head);
      }
      
      if(do_extract) {
        extracted = pmath_expr_set_item(extracted, ++ei, xi);
        x = pmath_expr_set_item(x, i, PMATH_UNDEFINED);
        continue;
      }
      
      pmath_unref(xi);
    }
    
    if(ei == xlen) {
      pmath_unref(xhead);
      pmath_unref(head);
      pmath_unref(expr);
      pmath_unref(x);
      return extracted;
    }
    
    if(ei > 0) {
      pmath_unref(xhead);
      pmath_unref(expr);
      
      x = _pmath_expr_shrink_associative(x, PMATH_UNDEFINED);
      expr = pmath_expr_get_item_range(extracted, 1, ei);
      pmath_unref(extracted);
      return TIMES(expr, FUNC(head, x));
    }
    
    pmath_unref(extracted);
  }
  
  pmath_unref(x);
  pmath_unref(xhead);
  pmath_unref(head);
  return expr;
}

static pmath_number_t number_abs(pmath_number_t x) {
  assert(pmath_is_number(x));
  
  if(pmath_is_double(x))
    return PMATH_FROM_DOUBLE(fabs(PMATH_AS_DOUBLE(x)));
    
  if(pmath_is_mpfloat(x)) {
    pmath_mpfloat_t result;
    if(pmath_refcount(x) > 1)
      result = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(x));
    else
      result = pmath_ref(x);
      
    if(!pmath_is_null(result)) {
      arb_abs(PMATH_AS_ARB(result), PMATH_AS_ARB(x));
      arf_get_mpfr(PMATH_AS_MP_VALUE(result), arb_midref(PMATH_AS_ARB(result)), MPFR_RNDN);
    }
    pmath_unref(x);
    return result;
  }
  
  if(pmath_number_sign(x) < 0)
    return pmath_number_neg(x);
  return x;
}

/** \brief Try to get the sign of a real numeric value x.
    \param x A numeric pMath object. It won't be freed.
    \return The real or complex sign if it can be proven, or PMATH_NULL if no prove is possible
 */
static pmath_t approximate_sign(pmath_t x) {
  pmath_thread_t me = pmath_thread_get_current();
  double precision;
  
  if(!me) 
    return PMATH_NULL;
  
  precision = FLINT_MIN(16, me->max_extra_precision);
  while(!pmath_aborting()) {
    pmath_t approx = pmath_set_precision(pmath_ref(x), precision);
    
    if(pmath_is_mpfloat(approx)) {
      if(arb_is_positive(PMATH_AS_ARB(approx))) {
        pmath_unref(approx);
        return INT(1);
      }
      if(arb_is_negative(PMATH_AS_ARB(approx))) {
        pmath_unref(approx);
        return INT(-1);
      }
      if(arb_is_zero(PMATH_AS_ARB(approx))) {
        pmath_unref(approx);
        return INT(0);
      }
    }
    else if(pmath_is_expr_of_len(approx, PMATH_SYMBOL_COMPLEX, 2)) {
      acb_t z;
      acb_init(z);
      slong prec;
      pmath_bool_t is_machine_prec;
      if(_pmath_complex_float_extract_acb(z, &prec, &is_machine_prec, approx)) {
        if(acb_is_real(z)) {
          if(arb_is_positive(acb_realref(z))) {
            pmath_unref(approx);
            acb_clear(z);
            return INT(1);
          }
          if(arb_is_negative(acb_realref(z))) {
            pmath_unref(approx);
            acb_clear(z);
            return INT(-1);
          }
          if(arb_is_zero(acb_realref(z))) {
            pmath_unref(approx);
            acb_clear(z);
            return INT(0);
          }
        }
        else if(arb_is_zero(acb_realref(z))) {
          if(arb_is_positive(acb_imagref(z))) {
            pmath_unref(approx);
            acb_clear(z);
            return COMPLEX(INT(0), INT(1));
          }
          if(arb_is_negative(acb_imagref(z))) {
            pmath_unref(approx);
            acb_clear(z);
            return COMPLEX(INT(0), INT(-1));
          }
        }
      }
      acb_clear(z);
    }
    else {
      pmath_unref(approx);
      break;
    }
    
    pmath_unref(approx);
    if(precision >= me->max_extra_precision)
      break;
    
    precision = FLINT_MIN(2 * precision, me->max_extra_precision);
  }
  
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_abs(pmath_expr_t expr) {
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_number(x)) {
    pmath_unref(expr);
    return number_abs(x);
  }
  
  if(_pmath_is_nonreal_complex_number(x)) {
    pmath_t re, im;
    acb_t z;
    slong prec;
    pmath_bool_t is_machine_prec;
    
    pmath_unref(expr);
    
    acb_init(z);
    if(_pmath_complex_float_extract_acb(z, &prec, &is_machine_prec, x)) {
      pmath_mpfloat_t result = _pmath_create_mp_float(prec);
      if(!pmath_is_null(result)) {
        pmath_unref(x);
        acb_abs(PMATH_AS_ARB(result), z, prec);
        acb_clear(z);
        
        if(is_machine_prec) {
          double d = arf_get_d(arb_midref(PMATH_AS_ARB(result)), ARF_RND_NEAR);
          if(isfinite(d)) {
            pmath_unref(result);
            return PMATH_FROM_DOUBLE(d);
          }
        }
        arf_get_mpfr(PMATH_AS_MP_VALUE(result), arb_midref(PMATH_AS_ARB(result)), MPFR_RNDN);
        return result;
      }
    }
    acb_clear(z);
    
    re = pmath_expr_get_item(x, 1);
    im = pmath_expr_get_item(x, 2);
    
    pmath_unref(x);
    if(pmath_number_sign(im) == 0) {
      pmath_unref(im);
      if(pmath_number_sign(re) >= 0)
        return re;
      return pmath_number_neg(re);
    }
    if(pmath_number_sign(re) == 0) {
      pmath_unref(re);
      if(pmath_number_sign(im) >= 0)
        return im;
      return pmath_number_neg(im);
    }
    return SQRT(PLUS(POW(re, INT(2)), POW(im, INT(2))));
  }
  
  if(pmath_is_expr_of_len(x, PMATH_SYMBOL_CONJUGATE, 1)) {
    expr = pmath_expr_set_item(expr, 1,
                               pmath_expr_get_item(x, 1));
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_is_expr_of(x, PMATH_SYMBOL_DIRECTEDINFINITY)) {
    pmath_unref(expr);
    pmath_unref(x);
    return pmath_ref(_pmath_object_pos_infinity);
  }
  
  if( pmath_equals(x, _pmath_object_overflow) ||
      pmath_equals(x, _pmath_object_underflow))
  {
    pmath_unref(expr);
    return x;
  }
  
  if(pmath_is_numeric(x)) {
    pmath_t sgn = approximate_sign(x);
    if(!pmath_is_null(sgn)) {
      pmath_unref(expr);
      if(pmath_same(sgn, PMATH_FROM_INT32(0))) {
        pmath_unref(x);
        return sgn;
      }
      return DIV(x, sgn);
    }
  }
  
  pmath_unref(x);
  return simplify_abs_sign(expr);
}

static pmath_number_t number_sign(pmath_number_t x) {
  int sign;
  assert(pmath_is_number(x));
  
  if(pmath_is_mpfloat(x)) {
    pmath_mpfloat_t result;
    if(arb_is_positive(PMATH_AS_ARB(x))) {
      pmath_unref(x);
      return PMATH_FROM_INT32(1);
    }
    if(arb_is_negative(PMATH_AS_ARB(x))) {
      pmath_unref(x);
      return PMATH_FROM_INT32(-1);
    }
    if(arb_is_zero(PMATH_AS_ARB(x))) {
      pmath_unref(x);
      return PMATH_FROM_INT32(0);
    }
    
    if(pmath_refcount(x) > 1)
      result = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(x));
    else
      result = pmath_ref(x);
      
    if(!pmath_is_null(result)) {
      arb_sgn(PMATH_AS_ARB(result), PMATH_AS_ARB(x));
      arf_get_mpfr(PMATH_AS_MP_VALUE(result), arb_midref(PMATH_AS_ARB(result)), MPFR_RNDN);
    }
    pmath_unref(x);
    return result;
  }
  
  sign = pmath_number_sign(x);
  pmath_unref(x);
  return PMATH_FROM_INT32(sign);
}

PMATH_PRIVATE pmath_t builtin_sign(pmath_expr_t expr) {
  pmath_t x;
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_number(x)) {
    pmath_unref(expr);
    return number_sign(x);
  }
  
  if(_pmath_is_nonreal_complex_number(x)) {
    pmath_t re, im;
    int im_sign, re_sign;
    acb_t z;
    slong prec;
    pmath_bool_t is_machine_prec;
    
    pmath_unref(expr);
    
    acb_init(z);
    if(_pmath_complex_float_extract_acb(z, &prec, &is_machine_prec, x)) {
      pmath_unref(x);
      acb_sgn(z, z, prec);
      x = _pmath_complex_new_from_acb(z, is_machine_prec ? -1 : prec);
      acb_clear(z);
      return x;
    }
    acb_clear(z);
    
    re = pmath_expr_get_item(x, 1);
    im = pmath_expr_get_item(x, 2);
    
    im_sign = pmath_number_sign(im);
    re_sign = pmath_number_sign(im);
    if(im_sign == 0 || re_sign == 0) {
      pmath_unref(re);
      pmath_unref(im);
      pmath_unref(x);
      return COMPLEX(INT(re_sign), INT(im_sign));
    }
    return DIV(x, SQRT(PLUS(POW(re, INT(2)), POW(im, INT(2)))));
  }
  
  if( pmath_equals(x, _pmath_object_overflow) ||
      pmath_equals(x, _pmath_object_underflow))
  {
    pmath_unref(expr);
    return x;
  }
  
  {
    pmath_t xinfdir = _pmath_directed_infinity_direction(x);
    if(!pmath_same(xinfdir, PMATH_NULL)) {
      pmath_unref(x);
      pmath_unref(expr);
      if(pmath_is_number(xinfdir) && pmath_number_sign(xinfdir) == 0) {
        pmath_unref(xinfdir);
        return pmath_ref(PMATH_SYMBOL_UNDEFINED);
      }
      
      return pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_SIGN), 1,
               xinfdir);
    }
  }
  
  if(pmath_is_numeric(x)) {
    pmath_t sgn = approximate_sign(x);
    if(!pmath_is_null(sgn)) {
      pmath_unref(expr);
      pmath_unref(x);
      return sgn;
    }
  }
  
  pmath_unref(x);
  return simplify_abs_sign(expr);
}
