#include <pmath-core/expressions-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>


extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_Piecewise;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_Undefined;

pmath_t builtin_piecewise(pmath_expr_t expr) {
  /* Piecewise({{val1, cond1}, {val2, cond2}, ...}, val)
     Piecewise(mat)  =  Piecewise(mat, 0)
   */
  pmath_bool_t all_definite;
  pmath_t matrix, fallback;
  size_t rows, cols, i;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  matrix = pmath_expr_get_item(expr, 1);
  if( !_pmath_is_matrix(matrix, &rows, &cols, FALSE) ||
      (cols != 2 &&
       rows != 0))
  {
    pmath_message(PMATH_NULL, "pairs", 1, matrix);
    return expr;
  }
  
  if(exprlen == 2)
    fallback = pmath_expr_get_item(expr, 2);
  else
    fallback = pmath_ref(pmath_System_Undefined);
    
  pmath_unref(expr);
  all_definite = TRUE;
  for(i = 1; i <= rows; ++i) {
    pmath_t pair = pmath_expr_get_item(matrix, i);
    pmath_t cond = pmath_evaluate(pmath_expr_get_item(pair, 2));
    
    if(pmath_same(cond, pmath_System_False)) {
      pmath_unref(cond);
      pmath_unref(pair);
      matrix = pmath_expr_set_item(matrix, i, PMATH_UNDEFINED);
      continue;
    }
    
    if(pmath_same(cond, pmath_System_True)) {
      pmath_unref(cond);
      pmath_unref(fallback);
      fallback = pmath_expr_get_item(pair, 1);
      pmath_unref(pair);
      
      if(all_definite) {
        pmath_unref(matrix);
        return fallback;
      }
      
      pair = matrix;
      matrix = pmath_expr_get_item_range(pair, 1, i - 1);
      pmath_unref(pair);
      break;
    }
    
    pair = pmath_expr_set_item(pair, 2, cond);
    matrix = pmath_expr_set_item(matrix, i, pair);
    all_definite = FALSE;
  }
  
  if(all_definite) {
    pmath_unref(matrix);
    return fallback;
  }
  
  matrix = pmath_expr_remove_all(matrix, PMATH_UNDEFINED);
  expr = pmath_expr_new_extended(
           pmath_ref(pmath_System_Piecewise), 2,
           matrix,
           fallback);
           
  _pmath_expr_update(expr);
  return expr;
}
