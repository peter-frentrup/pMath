#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-inline.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

PMATH_PRIVATE pmath_t builtin_diagonalmatrix(pmath_expr_t expr){
/* DiagonalMatrix(list, diag)
   DiagonalMatrix(list)         = DiagonalMatrix(list, 0)
   
   diag is the diagonal above the leading diagonal.
 */
  
  size_t len, i, j;
  intptr_t diag;
  pmath_t      mat, zero;
  pmath_expr_t list;
  
  len = pmath_expr_length(expr);
  if(len < 1 || len > 2){
    pmath_message_argxxx(len, 1, 2);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(list, PMATH_SYMBOL_LIST)){
    pmath_unref(list);
    return expr;
  }
  
  if(len == 2){
    pmath_t diag_obj = pmath_expr_get_item(expr, 2);
    
    if(!pmath_instance_of(diag_obj, PMATH_TYPE_INTEGER)
    || !pmath_integer_fits_si(diag_obj)){
      pmath_message(NULL, "intm", 2, pmath_ref(expr), pmath_integer_new_si(2));
      pmath_unref(list);
      pmath_unref(diag_obj);
      return expr;
    }
    
    diag = pmath_integer_get_si(diag_obj);
    pmath_unref(diag_obj);
  }
  else
    diag = 0;
    
  pmath_unref(expr);
  
  len = pmath_expr_length(list);
  if(diag < 0)
    len+= (size_t)-diag;
  else
    len+= (size_t)diag;
  
  zero = pmath_integer_new_si(0);
  mat = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), len);
  for(i = len;i > 0;--i){
    pmath_t row = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), len);
    
    if(diag >= 0){
      size_t k = i + (size_t)diag;
      
      for(j = len;j > k;--j)
        row = pmath_expr_set_item(row, j, pmath_ref(zero));
      
      row = pmath_expr_set_item(row, k, pmath_expr_get_item(list, i));
      
      for(j = 1;j < k;++j)
        row = pmath_expr_set_item(row, j, pmath_ref(zero));
    }
    else if(i > (size_t)-diag){
      size_t k = i - ((size_t)-diag);
      
      for(j = len;j > k;--j)
        row = pmath_expr_set_item(row, j, pmath_ref(zero));
      
      row = pmath_expr_set_item(row, k, pmath_expr_get_item(list, k));
      
      for(j = 1;j < k;++j)
        row = pmath_expr_set_item(row, j, pmath_ref(zero));
    }
    else{
      for(j = len;j > 0;--j)
        row = pmath_expr_set_item(row, j, pmath_ref(zero));
    }
    
    mat = pmath_expr_set_item(mat, i, row);
  }
  
  pmath_unref(list);
  pmath_unref(zero);
  
  return mat;
}
