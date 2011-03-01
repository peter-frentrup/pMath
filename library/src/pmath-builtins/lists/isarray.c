#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>
#include <pmath-language/patterns-private.h>


static pmath_bool_t all_nonlists(pmath_expr_t expr, size_t level){
  if(level > 0){
    size_t i;
    
    --level;
    for(i = pmath_expr_length(expr);i > 0;--i){
      if(!all_nonlists(pmath_expr_get_item(expr, i), level)){
        pmath_unref(expr);
        return FALSE;
      }
    }
    
    pmath_unref(expr);
    return TRUE;
  }
  
  if(pmath_is_expr_of(expr, PMATH_SYMBOL_LIST)){
    pmath_unref(expr);
    return FALSE;
  }
  
  pmath_unref(expr);
  return TRUE;
}

static pmath_bool_t all(pmath_expr_t expr, size_t level, pmath_t test){
  if(level > 0){
    size_t i;
    
    --level;
    for(i = pmath_expr_length(expr);i > 0;--i){
      if(!all(pmath_expr_get_item(expr, i), level, test)){
        pmath_unref(expr);
        return FALSE;
      }
    }
    
    pmath_unref(expr);
    return TRUE;
  }
  
  test = pmath_evaluate(pmath_expr_new_extended(pmath_ref(test), 1, expr));
  pmath_unref(test);
  return pmath_same(test, PMATH_SYMBOL_TRUE);
}

PMATH_PRIVATE pmath_t builtin_isarray(pmath_expr_t expr){
/* IsArray(obj)
   IsArray(obj, dimensionpattern)
   IsArray(obj, dimensionpattern, test)
 */
  pmath_t obj, dims;
  size_t levels;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 3){
    pmath_message_argxxx(exprlen, 1, 3);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(obj, PMATH_SYMBOL_LIST)){
    pmath_unref(obj);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FALSE);
  }
  
  dims = _pmath_dimensions(obj, SIZE_MAX);
  levels = pmath_expr_length(dims);
  pmath_unref(dims);
  if(!all_nonlists(pmath_ref(obj), levels)){
    pmath_unref(obj);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FALSE);
  }
  
  if(exprlen >= 2){
    pmath_t levels_obj = pmath_integer_new_size(levels);
    pmath_t pat        = pmath_expr_get_item(expr, 2);
    
    if(!_pmath_pattern_match(levels_obj, pat, NULL)){
      pmath_unref(obj);
      pmath_unref(levels_obj);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_FALSE);
    }
    
    pmath_unref(levels_obj);
    
    if(exprlen >= 3){
      pat = pmath_expr_get_item(expr, 3);
      
      if(!all(obj, levels, pat)){
        pmath_unref(pat);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FALSE);
      }
      
      obj = NULL;
      pmath_unref(pat);
    }
  }
  
  pmath_unref(obj);
  pmath_unref(expr);
  return pmath_ref(PMATH_SYMBOL_TRUE);
}

PMATH_PRIVATE pmath_t builtin_ismatrix(pmath_expr_t expr){
/* IsMatrix(obj)
   IsMatrix(obj, test)
 */
  pmath_t obj, dims;
  size_t levels;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(obj, PMATH_SYMBOL_LIST)){
    pmath_unref(obj);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FALSE);
  }
  
  dims = _pmath_dimensions(obj, SIZE_MAX);
  levels = pmath_expr_length(dims);
  pmath_unref(dims);
  if(levels != 2 || !all_nonlists(pmath_ref(obj), levels)){
    pmath_unref(obj);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FALSE);
  }
  
  if(exprlen >= 2){
    pmath_t test = pmath_expr_get_item(expr, 2);
    
    if(!all(obj, levels, test)){
      pmath_unref(test);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_FALSE);
    }
    
    obj = NULL;
    pmath_unref(test);
  }
  
  pmath_unref(obj);
  pmath_unref(expr);
  return pmath_ref(PMATH_SYMBOL_TRUE);
}

PMATH_PRIVATE pmath_t builtin_isvector(pmath_expr_t expr){
/* IsVector(obj)
   IsVector(obj, test)
 */
  pmath_t obj, dims;
  size_t levels;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(obj, PMATH_SYMBOL_LIST)){
    pmath_unref(obj);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FALSE);
  }
  
  dims = _pmath_dimensions(obj, SIZE_MAX);
  levels = pmath_expr_length(dims);
  pmath_unref(dims);
  if(levels != 1 || !all_nonlists(pmath_ref(obj), levels)){
    pmath_unref(obj);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FALSE);
  }
  
  if(exprlen >= 2){
    pmath_t test = pmath_expr_get_item(expr, 2);
    
    if(!all(obj, levels, test)){
      pmath_unref(test);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_FALSE);
    }
    
    obj = NULL;
    pmath_unref(test);
  }
  
  pmath_unref(obj);
  pmath_unref(expr);
  return pmath_ref(PMATH_SYMBOL_TRUE);
}
