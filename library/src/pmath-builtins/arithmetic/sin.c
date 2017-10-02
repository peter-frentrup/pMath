#include <pmath-core/numbers-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>


/** \brief Simplify the sine of a rational number considering the number's sign.
    \param expr The Sin(x) expression. It will be freed.
    \param x    The rational number argument. It will be freed.
    \return The simplified Cos(...) expression.
 */
static pmath_t sin_of_rational(pmath_expr_t expr, pmath_rational_t x) {
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

/** \brief Simplify the sine of an exact complex number.
    \param expr  Pointer to the Sin-expression. On success, this will be replaced by the evaluation result.
    \param x     The sine argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_sin_of_exact_complex(pmath_t *expr, pmath_t x) {
  pmath_t re, im;
  
  if(!pmath_is_expr_of_len(x, PMATH_SYMBOL_COMPLEX, 2))
    return FALSE;
    
  re = pmath_expr_get_item(x, 1);
  im = pmath_expr_get_item(x, 2);
  
  if(pmath_equals(re, PMATH_FROM_INT32(0))) {
    pmath_unref(re);
    pmath_unref(*expr);
    *expr = TIMES(
              COMPLEX(INT(0), INT(1)),
              FUNC(pmath_ref(PMATH_SYMBOL_SINH), im));
    return TRUE;
  }
  
  pmath_unref(re);
  pmath_unref(im);
  return FALSE;
}

/** \brief Evaluate Sin(ArcCos(u)), Sin(ArcTan(u)), etc.
    \param expr  Pointer to the Sin-expression. On success, this will be replaced by the evaluation result.
    \param x     The cosine argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_sin_of_triginverse(pmath_t *expr, pmath_t x) {
  pmath_t head, u;
  
  if(!pmath_is_expr(x) || pmath_expr_length(x) != 1)
    return FALSE;
    
  head = pmath_expr_get_item(x, 0);
  pmath_unref(head);
  u = pmath_expr_get_item(x, 1);
  
  if(pmath_same(head, PMATH_SYMBOL_ARCCOS)) {
    pmath_unref(*expr);
    *expr = SQRT(MINUS(INT(1), POW(u, INT(2)))); // Sqrt(1 - u^2)
    return TRUE;
  }
  if(pmath_same(head, PMATH_SYMBOL_ARCCOT)) {
    pmath_unref(*expr);
    // 1/u * 1/Sqrt(1 + 1/u^2))
    *expr = TIMES(INV(pmath_ref(u)), INVSQRT(PLUS(INT(1), POW(pmath_ref(u), INT(-2)))));
    pmath_unref(u);
    return TRUE;
  }
  if(pmath_same(head, PMATH_SYMBOL_ARCCSC)) {
    pmath_unref(*expr);
    *expr = INV(u); // 1/u
    return TRUE;
  }
  if(pmath_same(head, PMATH_SYMBOL_ARCSEC)) {
    pmath_unref(*expr);
    *expr = SQRT(MINUS(INT(1), POW(u, INT(-2)))); // Sqrt(1 - 1/u^2)
    return TRUE;
  }
  if(pmath_same(head, PMATH_SYMBOL_ARCSIN)) {
    pmath_unref(*expr);
    *expr = u;
    return TRUE;
  }
  if(pmath_same(head, PMATH_SYMBOL_ARCTAN)) {
    pmath_unref(*expr);
    // u / Sqrt(1 + u^2)
    *expr = TIMES(pmath_ref(u), INVSQRT(PLUS(INT(1), POW(pmath_ref(u), INT(2)))));
    pmath_unref(u);
    return TRUE;
  }
  pmath_unref(u);
  return FALSE;
}

/** \brief Simplify the sine of a product.
    \param expr  Pointer to the Sin-expression. On success, this will be replaced by the evaluation result.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_sin_of_product(pmath_t *expr) {
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
  
  if(_pmath_is_imaginary(&x)) {
    pmath_unref(fst);
    *expr = pmath_expr_set_item(*expr, 0, pmath_ref(PMATH_SYMBOL_SINH));
    *expr = pmath_expr_set_item(*expr, 1, x);
    *expr = TIMES(COMPLEX(INT(0), INT(1)), *expr);
    return TRUE;
  }
  if(pmath_expr_length(x) == 2) {
    pmath_t snd = pmath_expr_get_item(x, 2);
    pmath_unref(snd);
    
    if(pmath_same(snd, PMATH_SYMBOL_PI)) {
      pmath_t cmp;
      
      if(pmath_equals(fst, _pmath_one_half)) {
        pmath_unref(x);
        pmath_unref(fst);
        pmath_unref(*expr);
        *expr = INT(1);
        return TRUE;
      }
      
      cmp = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_LESS), 2,
                pmath_ref(fst),
                pmath_ref(_pmath_one_half)));
                
      pmath_unref(cmp);
      if( pmath_same(cmp, PMATH_SYMBOL_TRUE)  &&
          pmath_is_quotient(fst)              &&
          pmath_is_int32(PMATH_QUOT_NUM(fst)) &&
          pmath_is_int32(PMATH_QUOT_DEN(fst)) &&
          PMATH_AS_INT32(PMATH_QUOT_NUM(fst)) >= 0)
      {
        unsigned num = (unsigned)PMATH_AS_INT32(PMATH_QUOT_NUM(fst));
        unsigned den = (unsigned)PMATH_AS_INT32(PMATH_QUOT_DEN(fst));
        
        if(num <= den / 2)
          switch(den) {
            case 3:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              *expr = TIMES(ONE_HALF, SQRT(INT(3)));
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
                *expr = SQRT(PLUS(QUOT(5, 8), TIMES(QUOT(-1, 8), SQRT(INT(5)))));
              else
                *expr = SQRT(PLUS(QUOT(5, 8), TIMES(QUOT(1, 8), SQRT(INT(5)))));
              return TRUE;
              
            case 6:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              *expr = ONE_HALF;
              return TRUE;
              
            case 10:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              if(num == 1)
                *expr = PLUS(QUOT(-1, 4), TIMES(QUOT(1, 4), SQRT(INT(5))));
              else
                *expr = PLUS(QUOT(1, 4), TIMES(QUOT(1, 4), SQRT(INT(5))));
              return TRUE;
              
            case 12:
              pmath_unref(x);
              pmath_unref(fst);
              pmath_unref(*expr);
              if(num == 1)
                *expr = DIV(PLUS(INT(-1), SQRT(INT(3))), TIMES(INT(2), SQRT(INT(2))));
              else
                *expr = DIV(PLUS(INT(1), SQRT(INT(3))), TIMES(INT(2), SQRT(INT(2))));
              return TRUE;
          }
      }
      else if(pmath_same(cmp, PMATH_SYMBOL_FALSE)) { // 1/2 Pi <= x
        if(pmath_is_integer(fst)) {
          pmath_unref(fst);
          pmath_unref(x);
          pmath_unref(*expr);
          *expr = INT(0);
          return TRUE;
        }
        
        *expr = pmath_expr_set_item(*expr, 1, PMATH_NULL);
        
        cmp = pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_LESS), 2,
                  pmath_ref(fst),
                  PMATH_FROM_INT32(1)));
                  
        pmath_unref(cmp);
        if(pmath_same(cmp, PMATH_SYMBOL_TRUE)) { // 1/2 Pi <= x < Pi
          fst = MINUS(INT(1), fst);
          x     = pmath_expr_set_item(x,     1, fst);
          *expr = pmath_expr_set_item(*expr, 1, x); // return Sin((1-fst)*PI)
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
          *expr = NEG(pmath_expr_set_item(*expr, 1, x)); // return -Sin((2-fst)*PI)
          return TRUE;
        }
        
        cmp = pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_FLOOR), 2,
                  pmath_ref(fst),
                  PMATH_FROM_INT32(2)));
                  
        fst = PLUS(fst, TIMES(INT(-2), cmp));
        
        x     = pmath_expr_set_item(x,     1, fst);
        *expr = pmath_expr_set_item(*expr, 1, x); // return Sin((fst - Floor(fst, 2))*PI)
        return TRUE;
      }
    }
  }
  
  pmath_unref(fst);
  pmath_unref(x);
  return FALSE;
}

/** \brief Simplify the sine of a sum (containing Pi).
    \param expr  Pointer to the Sin-expression. On success, this will be replaced by the evaluation result.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_sin_of_sum(pmath_t *expr) {
  pmath_t x;
  size_t i;
  assert(pmath_is_expr(*expr));
  x = pmath_expr_get_item(*expr, 1);
  if(!pmath_is_expr_of(x, PMATH_SYMBOL_PLUS) || !_pmath_contains_symbol(x, PMATH_SYMBOL_PI)) {
    pmath_unref(x);
    return FALSE;
  }
    
  for(i = pmath_expr_length(x); i > 0; --i) {
    pmath_t tmp = pmath_expr_get_item(x, i);
    tmp = pmath_evaluate(DIV(tmp, pmath_ref(PMATH_SYMBOL_PI)));
    if(pmath_is_integer(tmp)) {
      tmp = POW(INT(-1), tmp);
      *expr = pmath_expr_set_item(*expr, 1, PMATH_NULL);
      x     = pmath_expr_set_item(x, i, PMATH_UNDEFINED);
      x     = pmath_expr_remove_all(x, PMATH_UNDEFINED);
      *expr = pmath_expr_set_item(*expr, 1, x);
      *expr = TIMES(tmp, *expr);
      return TRUE;
    }
    pmath_unref(tmp);
  }
  
  pmath_unref(x);
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_sin(pmath_expr_t expr) {
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_complex_try_evaluate_acb(&expr, x, acb_sin)) {
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_is_rational(x))
    return sin_of_rational(expr, x);
    
  if(pmath_same(x, PMATH_SYMBOL_PI)) {
    pmath_unref(expr);
    pmath_unref(x);
    return INT(0);
  }
  
  if(_pmath_try_simplify_degree(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  if(try_sin_of_exact_complex(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  if(try_sin_of_triginverse(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  pmath_unref(x);
  if(try_sin_of_product(&expr))
    return expr;
  if(try_sin_of_sum(&expr))
    return expr;
  return expr;
}
