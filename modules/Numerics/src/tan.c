#include "stdafx.h"
#include "util.h"
#include "trigonometry.h"

extern pmath_symbol_t pmath_System_ArcCos;
extern pmath_symbol_t pmath_System_ArcCot;
extern pmath_symbol_t pmath_System_ArcCsc;
extern pmath_symbol_t pmath_System_ArcSec;
extern pmath_symbol_t pmath_System_ArcSin;
extern pmath_symbol_t pmath_System_ArcTan;
extern pmath_symbol_t pmath_System_Tan;
extern pmath_symbol_t pmath_System_Tanh;

static pmath_t tan_of_rational(pmath_expr_t expr, pmath_rational_t x);
static pmath_bool_t try_tan_of_exact_complex(pmath_t *expr, pmath_t x);
static pmath_bool_t try_tan_of_triginverse(pmath_t *expr, pmath_t x);
static pmath_bool_t try_tan_of_product(pmath_t *expr);
static pmath_bool_t try_tan_of_sum(pmath_t *expr);

PMATH_PRIVATE pmath_t eval_System_Tan(pmath_expr_t expr) {
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_complex_try_evaluate_acb(&expr, x, acb_tan)) { // TODO: return CINFTY when applicable
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_is_rational(x))
    return tan_of_rational(expr, x);
    
  if(pmath_same(x, PMATH_SYMBOL_PI)) {
    pmath_unref(expr);
    pmath_unref(x);
    return INT(0);
  }
  
  if(pnum_try_simplify_degree(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  if(try_tan_of_exact_complex(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  if(try_tan_of_triginverse(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  pmath_unref(x);
  if(try_tan_of_product(&expr))
    return expr;
  if(try_tan_of_sum(&expr))
    return expr;
  return expr;
}

/** \brief Simplify the tangent of a rational number considering the number's sign.
    \param expr The Tan(x) expression. It will be freed.
    \param x    The rational number argument. It will be freed.
    \return The simplified Tan(...) expression.
 */
static pmath_t tan_of_rational(pmath_expr_t expr, pmath_rational_t x) {
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

/** \brief Simplify the tangent of an exact complex number.
    \param expr  Pointer to the Tan-expression. On success, this will be replaced by the evaluation result.
    \param x     The tangent argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_tan_of_exact_complex(pmath_t *expr, pmath_t x) {
  pmath_t re, im;
  
  if(!pmath_is_expr_of_len(x, PMATH_SYMBOL_COMPLEX, 2))
    return FALSE;
    
  re = pmath_expr_get_item(x, 1);
  im = pmath_expr_get_item(x, 2);
  
  if(pmath_equals(re, PMATH_FROM_INT32(0))) {
    pmath_unref(re);
    pmath_unref(*expr);
    *expr = TIMES(COMPLEX(INT(0), INT(1)), FUNC(pmath_ref(pmath_System_Tanh), im));
    return TRUE;
  }
  
  pmath_unref(re);
  pmath_unref(im);
  return FALSE;
}

/** \brief Evaluate Tan(ArcCos(u)), Tan(ArcTan(u)), etc.
    \param expr  Pointer to the Tan-expression. On success, this will be replaced by the evaluation result.
    \param x     The tangent argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_tan_of_triginverse(pmath_t *expr, pmath_t x) {
  pmath_t head, u;
  
  if(!pmath_is_expr(x) || pmath_expr_length(x) != 1)
    return FALSE;
    
  head = pmath_expr_get_item(x, 0);
  pmath_unref(head);
  u = pmath_expr_get_item(x, 1);
  if(pmath_same(head, pmath_System_ArcCos)) {
    pmath_unref(*expr);
    // Sqrt(1 - u^2)/u
    *expr = DIV(SQRT(MINUS(INT(1), POW(pmath_ref(u), INT(2)))), pmath_ref(u));
    pmath_unref(u);
    return TRUE;
  }
  if(pmath_same(head, pmath_System_ArcCot)) {
    pmath_unref(*expr);
    *expr = INV(u); // 1/u
    return TRUE;
  }
  if(pmath_same(head, pmath_System_ArcCsc)) {
    pmath_unref(*expr);
    // 1/u * 1/Sqrt(1 - 1/u^2))
    *expr = TIMES(INV(pmath_ref(u)), INVSQRT(MINUS(INT(1), POW(pmath_ref(u), INT(-2)))));
    pmath_unref(u);
    return TRUE;
  }
  if(pmath_same(head, pmath_System_ArcSec)) {
    pmath_unref(*expr);
    // u Sqrt(1 - 1/u^2)
    *expr = TIMES(pmath_ref(u), SQRT(MINUS(INT(1), POW(pmath_ref(u), INT(-2)))));
    pmath_unref(u);
    return TRUE;
  }
  if(pmath_same(head, pmath_System_ArcSin)) {
    pmath_unref(*expr);
    // u / Sqrt(1 - u^2)
    *expr = DIV(pmath_ref(u), SQRT(MINUS(INT(1), POW(pmath_ref(u), INT(2)))));
    pmath_unref(u);
    return TRUE;
  }
  if(pmath_same(head, pmath_System_ArcTan)) {
    pmath_unref(*expr);
    *expr = u;
    return TRUE;
  }
  pmath_unref(u);
  return FALSE;
}

/** \brief Simplify the tangent of a product.
    \param expr  Pointer to the Tan-expression. On success, this will be replaced by the evaluation result.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_tan_of_product(pmath_t *expr) {
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
    *expr = pmath_expr_set_item(*expr, 0, pmath_ref(pmath_System_Tanh));
    *expr = pmath_expr_set_item(*expr, 1, x);
    pmath_unref(fst);
    *expr = TIMES(COMPLEX(INT(0), INT(1)), *expr);
    return TRUE;
  }
  
  if(pmath_expr_length(x) == 2) {
    pmath_t snd = pmath_expr_get_item(x, 2);
    pmath_unref(snd);
    
    if(pmath_same(snd, PMATH_SYMBOL_PI)) {
      pmath_t cmp;
      int32_t num;
      int32_t den;
      
      if(pnum_equals_quotient(fst, 1, 2)) {
        pmath_unref(x);
        pmath_unref(fst);
        pmath_unref(*expr);
        *expr = CINFTY;
        return TRUE;
      }
      
      cmp = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_LESS), 2,
                pmath_ref(fst),
                ONE_HALF));
                
      pmath_unref(cmp);
      if( pmath_same(cmp, PMATH_SYMBOL_TRUE)       &&
          pnum_get_small_rational(fst, &num, &den) &&
          num >= 0)
      {
        if(num <= den / 2)
          switch(den) {
            case 3:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              *expr = SQRT(INT(3));
              return TRUE;
            case 4:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              *expr = INT(1);
              return TRUE;
            case 5:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              if(num == 1)
                *expr = SQRT(PLUS(INT(5), TIMES(INT(-2), SQRT(INT(5)))));
              else
                *expr = SQRT(PLUS(INT(5), TIMES(INT(2), SQRT(INT(5)))));
              return TRUE;
            case 6:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              *expr = INVSQRT(INT(3));
              return TRUE;
            case 10:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              if(num == 1)
                *expr = SQRT(PLUS(INT(1), TIMES(INT(-2), INVSQRT(INT(5)))));
              else
                *expr = SQRT(PLUS(INT(1), TIMES(INT(2), INVSQRT(INT(5)))));
              return TRUE;
            case 12:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              if(num == 1)
                *expr = MINUS(INT(2), SQRT(INT(3)));
              else
                *expr = PLUS(INT(2), SQRT(INT(3)));
              return TRUE;
          }
      }
      else if(pmath_same(cmp, PMATH_SYMBOL_FALSE)) { // 1/2 Pi <= x
        *expr = pmath_expr_set_item(*expr, 1, PMATH_NULL);
        
        cmp = pmath_evaluate(
                FUNC2(pmath_ref(PMATH_SYMBOL_LESS), pmath_ref(fst), INT(1)));
                
        pmath_unref(cmp);
        if(pmath_same(cmp, PMATH_SYMBOL_TRUE)) { // 1/2 Pi <= x < Pi
          fst = MINUS(INT(1), fst);
          x = pmath_expr_set_item(x, 1, fst);
          *expr = NEG(pmath_expr_set_item(*expr, 1, x)); // return -Tan((1-fst)*PI)
          return TRUE;
        }
        
        cmp = pmath_evaluate(
                FUNC2(pmath_ref(PMATH_SYMBOL_LESS), pmath_ref(fst), INT(2)));
                
        pmath_unref(cmp);
        if(pmath_same(cmp, PMATH_SYMBOL_TRUE)) { // Pi <= x < 2 Pi
          fst = PLUS(fst, INT(-1));
          x = pmath_expr_set_item(x, 1, fst);
          *expr = pmath_expr_set_item(*expr, 1, x); // return Tan((fst-1)*PI)
          return TRUE;
        }
        
        cmp = pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_FLOOR), 2,
                  pmath_ref(fst),
                  PMATH_FROM_INT32(2)));
                  
        fst = PLUS(fst, TIMES(INT(-2), cmp));
        
        x     = pmath_expr_set_item(x,     1, fst);
        *expr = pmath_expr_set_item(*expr, 1, x); // return Tan((fst - Floor(fst, 2))*PI)
        return TRUE;
      }
    }
  }
  
  pmath_unref(fst);
  pmath_unref(x);
  return FALSE;
}

/** \brief Simplify the tangent of a sum (containing Pi).
    \param expr  Pointer to the Tan-expression. On success, this will be replaced by the evaluation result.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_tan_of_sum(pmath_t *expr) {
  pmath_t x;
  size_t i;
  assert(pmath_is_expr(*expr));
  x = pmath_expr_get_item(*expr, 1);
  if(!pmath_is_expr_of(x, PMATH_SYMBOL_PLUS) || !pnum_contains_nonhead_symbol(x, PMATH_SYMBOL_PI)) {
    pmath_unref(x);
    return FALSE;
  }
  
  for(i = pmath_expr_length(x); i > 0; --i) {
    pmath_t tmp = pmath_expr_get_item(x, i);
    
    tmp = pmath_evaluate(DIV(tmp, pmath_ref(PMATH_SYMBOL_PI)));
    
    if(pmath_is_integer(tmp)) {
      pmath_unref(tmp);
      *expr = pmath_expr_set_item(*expr, 1, PMATH_NULL);
      x     = pmath_expr_set_item(x, i, PMATH_UNDEFINED);
      x     = pmath_expr_remove_all(x, PMATH_UNDEFINED);
      *expr = pmath_expr_set_item(*expr, 1, x);
      return TRUE;
    }
    
    if(pmath_is_quotient(tmp)) {
      pmath_t den = pmath_rational_denominator(tmp);
      
      if(pmath_equals(den, PMATH_FROM_INT32(2))) {
        pmath_unref(den);
        pmath_unref(tmp);
        pmath_unref(*expr);
        x = pmath_expr_set_item(x, i, PMATH_UNDEFINED);
        x = pmath_expr_remove_all(x, PMATH_UNDEFINED);
        *expr = NEG(FUNC(pmath_ref(PMATH_SYMBOL_TAN), x));
        return TRUE;
      }
      
      pmath_unref(den);
    }
    
    pmath_unref(tmp);
  }
  
  pmath_unref(x);
  return FALSE;
}
