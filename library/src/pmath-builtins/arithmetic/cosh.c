#include <pmath-core/numbers-private.h>
#include <pmath-core/intervals-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>


/** \brief Simplify the hyperbolic cosine of a rational number considering the number's sign.
    \param expr The Cosh(x) expression. It will be freed.
    \param x    The rational number argument. It will be freed.
    \return The simplified Cosh(...) expression.
 */
static pmath_t cosh_of_rational(pmath_expr_t expr, pmath_rational_t x) {
  int sign;
  assert(pmath_is_rational(x));
  sign = pmath_number_sign(x);
  if(sign < 0) {
    expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
    return pmath_expr_set_item(expr, 1, pmath_number_neg(x));
  }
  if(sign == 0) {
    pmath_unref(expr);
    pmath_unref(x);
    return INT(1);
  }
  pmath_unref(x);
  return expr;
}

/** \brief Simplify the hyperbolic cosine of an exact complex number.
    \param expr  Pointer to the Cosh-expression. On success, this will be replaced by the evaluation result.
    \param x     The hyperbolic cosine argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_cosh_of_exact_complex(pmath_t *expr, pmath_t x) {
  pmath_t re, im;
  
  if(!pmath_is_expr_of_len(x, PMATH_SYMBOL_COMPLEX, 2))
    return FALSE;
    
  re = pmath_expr_get_item(x, 1);
  im = pmath_expr_get_item(x, 2);
  
  if(pmath_equals(re, PMATH_FROM_INT32(0))) {
    pmath_unref(re);
    pmath_unref(x);
    pmath_unref(*expr);
    *expr = FUNC(pmath_ref(PMATH_SYMBOL_COS), im);
    return TRUE;
  }
  
  pmath_unref(re);
  pmath_unref(im);
  return FALSE;
}

/** \brief Simplify the hyperbolic cosine of a product.
    \param expr  Pointer to the Cosh-expression. On success, this will be replaced by the evaluation result.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_cosh_of_product(pmath_t *expr) {
  pmath_t x, fst;
  assert(pmath_is_expr(*expr));
  x = pmath_expr_get_item(*expr, 1);
  if(!pmath_is_expr_of(x, PMATH_SYMBOL_TIMES)) {
    pmath_unref(x);
    return FALSE;
  }
  
  fst = pmath_expr_get_item(x, 1);
  
  if(pmath_is_number(fst)) {
    if(pmath_number_sign(fst) < 0) {
      *expr = pmath_expr_set_item(*expr, 1, PMATH_NULL);
      *expr = pmath_expr_set_item(
                *expr, 1,
                pmath_expr_set_item(
                  x, 1,
                  pmath_number_neg(fst)));
      return TRUE;
    }
  }
  if(_pmath_is_imaginary(&x)) {
    *expr = pmath_expr_set_item(*expr, 0, pmath_ref(PMATH_SYMBOL_COS));
    *expr = pmath_expr_set_item(*expr, 1, x);
    pmath_unref(fst);
    return TRUE;
  }
  
  pmath_unref(fst);
  pmath_unref(x);
  return FALSE;
}

/** \brief Evaluate Cosh(ArcCosh(u)), Cosh(ArcTanh(u)), etc.
    \param expr  Pointer to the Cosh-expression. On success, this will be replaced by the evaluation result.
    \param x     The hyperbolic cosine argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_cosh_of_hypinverse(pmath_t *expr, pmath_t x) {
  pmath_t head, u;
  
  if(!pmath_is_expr(x) || pmath_expr_length(x) != 1)
    return FALSE;
    
  head = pmath_expr_get_item(x, 0);
  pmath_unref(head);
  u = pmath_expr_get_item(x, 1);
  
  if(pmath_same(head, PMATH_SYMBOL_ARCCOSH)) {
    pmath_unref(*expr);
    *expr = u;
    return TRUE;
  }
  if(pmath_same(head, PMATH_SYMBOL_ARCCOTH)) {
    pmath_unref(*expr);
    *expr = INVSQRT(MINUS(INT(1), POW(u, INT(-2)))); // 1/Sqrt(1 - 1/u^2)
    return TRUE;
  }
  if(pmath_same(head, PMATH_SYMBOL_ARCCSCH)) {
    pmath_unref(*expr);
    *expr = SQRT(PLUS(INT(1), POW(u, INT(-2)))); // Sqrt(1 + 1/u^2)
    return TRUE;
  }
  if(pmath_same(head, PMATH_SYMBOL_ARCSECH)) {
    pmath_unref(*expr);
    *expr = INV(u); // 1/u
    return TRUE;
  }
  if(pmath_same(head, PMATH_SYMBOL_ARCSINH)) {
    pmath_unref(*expr);
    *expr = SQRT(PLUS(INT(1), POW(u, INT(2)))); // Sqrt(1 + u^2)
    return TRUE;
  }
  if(pmath_same(head, PMATH_SYMBOL_ARCTANH)) {
    pmath_unref(*expr);
    *expr = INVSQRT(MINUS(INT(1), POW(u, INT(2)))); // 1 / Sqrt(1 - u^2)
    return TRUE;
  }
  pmath_unref(u);
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_cosh(pmath_expr_t expr) {
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(_pmath_complex_try_evaluate_acb(&expr, x, acb_cosh)) {
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_is_rational(x))
    return cosh_of_rational(expr, x);
    
  if(try_cosh_of_exact_complex(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  if(try_cosh_of_hypinverse(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  pmath_unref(x);
  if(try_cosh_of_product(&expr))
    return expr;
  return expr;
}
