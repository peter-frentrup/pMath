#include "stdafx.h"
#include "util.h"

extern pmath_symbol_t pmath_System_ArcCosh;
extern pmath_symbol_t pmath_System_ArcCoth;
extern pmath_symbol_t pmath_System_ArcCsch;
extern pmath_symbol_t pmath_System_ArcSech;
extern pmath_symbol_t pmath_System_ArcSinh;
extern pmath_symbol_t pmath_System_ArcTanh;
extern pmath_symbol_t pmath_System_Tan;
extern pmath_symbol_t pmath_System_Tanh;

static pmath_t tanh_of_rational(pmath_expr_t expr, pmath_rational_t x);
static pmath_bool_t try_tanh_of_exact_complex(pmath_t *expr, pmath_t x);
static pmath_bool_t try_tanh_of_hypinverse(pmath_t *expr, pmath_t x);
static pmath_bool_t try_tanh_of_product(pmath_t *expr);

PMATH_PRIVATE pmath_t eval_System_Tanh(pmath_expr_t expr) {
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_complex_try_evaluate_acb(&expr, x, acb_tanh)) { // TODO: return CINFTY when applicable
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_is_rational(x))
    return tanh_of_rational(expr, x);
    
  if(try_tanh_of_exact_complex(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  if(try_tanh_of_hypinverse(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  pmath_unref(x);
  if(try_tanh_of_product(&expr))
    return expr;
  return expr;
}

/** \brief Simplify the hyperolic tangent of a rational number considering the number's sign.
    \param expr The Tanh(x) expression. It will be freed.
    \param x    The rational number argument. It will be freed.
    \return The simplified Tanh(...) expression.
 */
static pmath_t tanh_of_rational(pmath_expr_t expr, pmath_rational_t x) {
  int sign;
  assert(pmath_is_rational(x));
  sign = pmath_number_sign(x);
  if(sign < 0) {
    expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
    return NEG(pmath_expr_set_item(expr, 1, pmath_number_neg(x)));
  }
  if(sign == 0) {
    pmath_unref(expr);
    return x;
  }
  pmath_unref(x);
  return expr;
}

/** \brief Simplify the hyperbolic tangent of an exact complex number.
    \param expr  Pointer to the Tanh-expression. On success, this will be replaced by the evaluation result.
    \param x     The hyperbolic tangent argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_tanh_of_exact_complex(pmath_t *expr, pmath_t x) {
  pmath_t re, im;
  
  if(!pmath_is_expr_of_len(x, PMATH_SYMBOL_COMPLEX, 2))
    return FALSE;
    
  re = pmath_expr_get_item(x, 1);
  im = pmath_expr_get_item(x, 2);
  
  if(pmath_equals(re, PMATH_FROM_INT32(0))) {
    pmath_unref(re);
    pmath_unref(*expr);
    *expr = TIMES(COMPLEX(INT(0), INT(1)), FUNC(pmath_ref(pmath_System_Tan), im));
    return TRUE;
  }
  
  pmath_unref(re);
  pmath_unref(im);
  return FALSE;
}

/** \brief Evaluate Tanh(ArcCosh(u)), Tanh(ArcTanh(u)), etc.
    \param expr  Pointer to the Tanh-expression. On success, this will be replaced by the evaluation result.
    \param x     The hyperbolic tangent argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_tanh_of_hypinverse(pmath_t *expr, pmath_t x) {
  pmath_t head, u;
  
  if(!pmath_is_expr(x) || pmath_expr_length(x) != 1)
    return FALSE;
    
  head = pmath_expr_get_item(x, 0);
  pmath_unref(head);
  u = pmath_expr_get_item(x, 1);
  if(pmath_same(head, pmath_System_ArcCosh)) {
    pmath_unref(*expr);
    // (1+u) Sqrt((-1+u)/(1+u)) /u
    x = PLUS(INT(1), pmath_ref(u));
    *expr = TIMES3(
              pmath_ref(x),
              SQRT(DIV(
                     PLUS(INT(-1), pmath_ref(u)),
                     pmath_ref(x))),
              INV(pmath_ref(u)));
    pmath_unref(x);
    pmath_unref(u);
    return TRUE;
  }
  if(pmath_same(head, pmath_System_ArcCoth)) {
    pmath_unref(*expr);
    *expr = INV(u); // 1/u
    return TRUE;
  }
  if(pmath_same(head, pmath_System_ArcCsch)) {
    pmath_unref(*expr);
    // 1 / (Sqrt(1 + 1/u^2) u)
    *expr = INV(
              TIMES(
                SQRT(
                  PLUS(
                    INT(1),
                    POW(
                      pmath_ref(u),
                      INT(-2)))),
                pmath_ref(u)));
    pmath_unref(u);
    return TRUE;
  }
  if(pmath_same(head, pmath_System_ArcSech)) {
    pmath_unref(*expr);
    // (1+u) Sqrt((1-u)/(1+u))
    x = PLUS(INT(1), pmath_ref(u));
    *expr = TIMES(
              pmath_ref(x),
              SQRT(DIV(
                     MINUS(INT(1), u),
                     pmath_ref(x))));
    pmath_unref(x);
    return TRUE;
  }
  if(pmath_same(head, pmath_System_ArcSinh)) {
    pmath_unref(*expr);
    // u / Sqrt(1 + u^2)
    *expr = TIMES(pmath_ref(u), INVSQRT(PLUS(INT(1), POW(pmath_ref(u), INT(2)))));
    pmath_unref(u);
    return TRUE;
  }
  if(pmath_same(head, pmath_System_ArcTanh)) {
    pmath_unref(*expr);
    *expr = u;
    return TRUE;
  }
  pmath_unref(u);
  return FALSE;
}

/** \brief Simplify the hyperbolic tangent of a product.
    \param expr  Pointer to the Tanh-expression. On success, this will be replaced by the evaluation result.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_tanh_of_product(pmath_t *expr) {
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
      *expr = NEG(pmath_expr_set_item(*expr, 1, pmath_expr_set_item(x, 1, pmath_number_neg(fst))));
      return TRUE;
    }
  }
  if(pnum_is_imaginary(&x)) {
    *expr = pmath_expr_set_item(*expr, 0, pmath_ref(pmath_System_Tan));
    *expr = pmath_expr_set_item(*expr, 1, x);
    pmath_unref(fst);
    *expr = TIMES(COMPLEX(INT(0), INT(1)), *expr);
    return TRUE;
  }
  
  pmath_unref(fst);
  pmath_unref(x);
  return FALSE;
}
