#include <pmath-core/numbers-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>


// x will be freed, x may be PMATH_NULL
static pmath_mpfloat_t mp_arctan(pmath_mpfloat_t x) {
  pmath_mpfloat_t val;
  
  if(pmath_is_null(x))
    return PMATH_NULL;
  
  assert(pmath_is_mpfloat(x));
  
  val = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(x)));
  
  if(pmath_is_null(val)) {
    pmath_unref(x);
    return val;
  }
  
  mpfr_atan(
    PMATH_AS_MP_VALUE(val),
    PMATH_AS_MP_VALUE(x),
    MPFR_RNDN);
    
  pmath_unref(x);
  
  val = _pmath_float_exceptions(val);
  
  return val;
}

PMATH_PRIVATE pmath_t builtin_arctan(pmath_expr_t expr) {
  pmath_t x;
  int xclass;
  pmath_thread_t me = pmath_thread_get_current();
  
  if(!me)
    return expr;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  
  if(pmath_equals(x, PMATH_FROM_INT32(0))) {
    pmath_unref(expr);
    return x;
  }
  
  if(pmath_is_double(x)) {
    double d = PMATH_AS_DOUBLE(x);
    pmath_unref(expr);
    pmath_unref(x);
    
    return PMATH_FROM_DOUBLE(atan(d));
  }
  
  if(pmath_is_mpfloat(x)) {
    pmath_unref(expr);
    
    x = mp_arctan(x);
    x = _pmath_float_exceptions(x);
    return x;
  }
  
  if(pmath_is_expr(x)) {
    pmath_t head = pmath_expr_get_item(x, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_SYMBOL_TIMES)) {
      pmath_t fst = pmath_expr_get_item(x, 1);
      
      if(pmath_is_number(fst)) {
        if(pmath_number_sign(fst) < 0) {
          x = pmath_expr_set_item(x, 1, pmath_number_neg(fst));
          expr = pmath_expr_set_item(expr, 1, x);
          return NEG(expr);
        }
      }
      
      pmath_unref(fst);
      
      if(_pmath_is_imaginary(&x)) {
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_ARCTANH));
        expr = pmath_expr_set_item(expr, 1, x);
        return TIMES(COMPLEX(INT(0), INT(1)), expr);
      }
    }
  }
  
  xclass = _pmath_number_class(x);
  
  if(xclass & PMATH_CLASS_ZERO) {
    pmath_unref(expr);
    return x;
  }
  
  if(xclass & PMATH_CLASS_POSONE) {
    pmath_unref(expr);
    pmath_unref(x);
    return TIMES(QUOT(1, 4), pmath_ref(PMATH_SYMBOL_PI));
  }
  
  if(xclass & PMATH_CLASS_NEG) {
    x = NEG(x);
    expr = pmath_expr_set_item(expr, 1, x);
    return NEG(expr);
  }
  
  if(xclass & PMATH_CLASS_INF) {
    pmath_t infdir = _pmath_directed_infinity_direction(x);
    if(!pmath_same(infdir, PMATH_NULL)) {
      pmath_unref(expr);
      pmath_unref(x);
      
      if(pmath_equals(infdir, PMATH_FROM_INT32(0))) {
        pmath_unref(infdir);
        return pmath_ref(PMATH_SYMBOL_UNDEFINED);
      }
      
      return TIMES3(ONE_HALF, pmath_ref(PMATH_SYMBOL_PI), INV(infdir));
    }
  }
  
  if(xclass & PMATH_CLASS_COMPLEX) {
    pmath_t re = PMATH_NULL;
    pmath_t im = PMATH_NULL;
    
    if(_pmath_re_im(x, &re, &im)) {
      if(pmath_equals(re, PMATH_FROM_INT32(0))) {
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(x);
        
        return TIMES(COMPLEX(INT(0), INT(1)), FUNC(pmath_ref(PMATH_SYMBOL_ARCTANH), im));
      }
      
      if(pmath_is_mpfloat(re) || pmath_is_mpfloat(im)) {
        pmath_unref(re);
        pmath_unref(im);
        pmath_unref(expr);
        
        // ArcTan(x) = -1/2 I Log((1 + I x) / (1 - I x))
        expr = TIMES(
                 COMPLEX(INT(0), QUOT(-1, 2)),
                 LOG(DIV(
                       PLUS(INT(1), TIMES(COMPLEX(INT(0), INT(1)), pmath_ref(x))),
                       PLUS(INT(1), TIMES(COMPLEX(INT(0), INT(-1)), pmath_ref(x))))));
        pmath_unref(x);
        return expr;
      }
    }
    
    pmath_unref(re);
    pmath_unref(im);
  }
  
  pmath_unref(x);
  return expr;
}
