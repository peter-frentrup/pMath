#include "stdafx.h"
#include "util.h"
#include "trigonometry.h"
#include "trig-of-triginverse.h"

extern pmath_symbol_t pmath_System_Cos;
extern pmath_symbol_t pmath_System_Cosh;

static pmath_t cos_of_rational(pmath_expr_t expr, pmath_rational_t x);
static pmath_bool_t try_cos_of_exact_complex(pmath_t *expr, pmath_t x);
static pmath_bool_t try_cos_of_product(pmath_t *expr);
static pmath_bool_t try_cos_of_sum(pmath_t *expr);

PMATH_PRIVATE pmath_t eval_System_Cos(pmath_expr_t expr) {
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_complex_try_evaluate_acb(&expr, x, acb_cos)) {
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_is_rational(x))
    return cos_of_rational(expr, x);
    
  if(pmath_same(x, PMATH_SYMBOL_PI)) {
    pmath_unref(expr);
    pmath_unref(x);
    return INT(-1);
  }
  
  if(pnum_try_simplify_degree(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  if(try_cos_of_exact_complex(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  if(try_cos_of_triginverse(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  pmath_unref(x);
  if(try_cos_of_product(&expr))
    return expr;
  if(try_cos_of_sum(&expr))
    return expr;
    
  return expr;
}

/** \brief Simplify the cosine of a rational number considering the number's sign.
    \param expr The Cos(x) expression. It will be freed.
    \param x    The rational number argument. It will be freed.
    \return The simplified Cos(...) expression.
 */
static pmath_t cos_of_rational(pmath_expr_t expr, pmath_rational_t x) {
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

/** \brief Simplify the cosine of an exact complex number.
    \param expr  Pointer to the Cos-expression. On success, this will be replaced by the evaluation result.
    \param x     The cosine argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_cos_of_exact_complex(pmath_t *expr, pmath_t x) {
  pmath_t re, im;
  
  if(!pmath_is_expr_of_len(x, PMATH_SYMBOL_COMPLEX, 2))
    return FALSE;
    
  re = pmath_expr_get_item(x, 1);
  im = pmath_expr_get_item(x, 2);
  
  if(pmath_equals(re, PMATH_FROM_INT32(0))) {
    pmath_unref(re);
    pmath_unref(*expr);
    *expr = FUNC(pmath_ref(pmath_System_Cosh), im);
    return TRUE;
  }
  
  pmath_unref(re);
  pmath_unref(im);
  return FALSE;
}

/** \brief Simplify the cosine of a product.
    \param expr  Pointer to the Cos-expression. On success, this will be replaced by the evaluation result.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_cos_of_product(pmath_t *expr) {
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
      *expr = pmath_expr_set_item(*expr, 1, pmath_expr_set_item(x, 1, pmath_number_neg(fst)));
      return TRUE;
    }
  }
  if(pnum_is_imaginary(&x)) {
    *expr = pmath_expr_set_item(*expr, 0, pmath_ref(pmath_System_Cosh));
    *expr = pmath_expr_set_item(*expr, 1, x);
    pmath_unref(fst);
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
        *expr = INT(0);
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
              *expr = ONE_HALF;
              return TRUE;
              
            case 4:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              *expr = INVSQRT(INT(2));
              return TRUE;
              
            case 5:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              if(num == 1)
                *expr = PLUS(QUOT(1, 4), TIMES(QUOT(1, 4), SQRT(INT(5))));
              else
                *expr = PLUS(QUOT(-1, 4), TIMES(QUOT(1, 4), SQRT(INT(5))));
              return TRUE;
              
            case 6:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              *expr = TIMES(ONE_HALF, SQRT(INT(3)));
              return TRUE;
              
            case 10:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              if(num == 1)
                *expr = SQRT(PLUS(QUOT(5, 8), TIMES(QUOT(1, 8), SQRT(INT(5)))));
              else
                *expr = SQRT(PLUS(QUOT(5, 8), TIMES(QUOT(-1, 8), SQRT(INT(5)))));
              return TRUE;
              
            case 12:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              if(num == 1)
                *expr = TIMES3(ONE_HALF, PLUS(INT(1), SQRT(INT(3))), INVSQRT(INT(2)));
              else
                *expr = TIMES3(ONE_HALF, PLUS(INT(-1), SQRT(INT(3))), INVSQRT(INT(2)));
              return TRUE;
          }
      }
      else if(pmath_same(cmp, PMATH_SYMBOL_FALSE)) { // 1/2 Pi <= x
        *expr = pmath_expr_set_item(*expr, 1, PMATH_NULL);
        
        cmp = pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_LESS), 2,
                  pmath_ref(fst),
                  PMATH_FROM_INT32(1)));
                  
        pmath_unref(cmp);
        if(pmath_same(cmp, PMATH_SYMBOL_TRUE)) { // 1/2 Pi <= x < Pi
          fst = MINUS(INT(1), fst);
          
          x = pmath_expr_set_item(x, 1, fst);
          *expr = NEG(pmath_expr_set_item(*expr, 1, x)); // return -Cos((1-fst)*PI)
          return TRUE;
        }
        
        cmp = pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_LESS), 2,
                  pmath_ref(fst),
                  PMATH_FROM_INT32(2)));
                  
        pmath_unref(cmp);
        if(pmath_same(cmp, PMATH_SYMBOL_TRUE)) { // Pi <= x < 2 Pi
          fst = MINUS(INT(2), fst);
          
          x = pmath_expr_set_item(x, 1, fst);
          *expr = pmath_expr_set_item(*expr, 1, x); // return Cos((2-fst)*PI)
          return TRUE;
        }
        
        cmp = pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_FLOOR), 2,
                  pmath_ref(fst),
                  PMATH_FROM_INT32(2)));
                  
        fst = MINUS(fst, cmp);
        
        x    = pmath_expr_set_item(x,    1, fst);
        *expr = pmath_expr_set_item(*expr, 1, x); // return Cos((fst - Floor(fst, 2))*PI)
        return TRUE;
      }
    }
  }
  
  pmath_unref(fst);
  pmath_unref(x);
  return FALSE;
}

/** \brief Simplify the cosine of a sum (containing Pi).
    \param expr  Pointer to the Cos-expression. On success, this will be replaced by the evaluation result.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_cos_of_sum(pmath_t *expr) {
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
      tmp = POW(INT(-1), tmp);
      
      *expr = pmath_expr_set_item(*expr, 1, PMATH_NULL);
      x = pmath_expr_set_item(x, i, PMATH_UNDEFINED);
      x = pmath_expr_remove_all(x, PMATH_UNDEFINED);
      *expr = pmath_expr_set_item(*expr, 1, x);
      
      *expr = TIMES(tmp, *expr);
      return TRUE;
    }
    
    pmath_unref(tmp);
  }
  
  pmath_unref(x);
  return FALSE;
}
