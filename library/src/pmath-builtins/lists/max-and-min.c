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
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>

static pmath_expr_t find_best(
  pmath_expr_t list,  // will be freed
  pmath_t      cmp,   // wont be freed
  pmath_t      head   // wont be freed
){
  size_t i, j, len = pmath_expr_length(list);
  pmath_t test, cmp_expr;
  
  for(i = 1;i <= len;++i){
    pmath_t item = pmath_expr_get_item(list, i);
    
    if(pmath_is_expr_of(item, PMATH_SYMBOL_LIST)
    || pmath_is_expr_of(item, head)){
      list = pmath_expr_set_item(list, i, NULL);
      
      item = find_best(item, cmp, head);
      
      if(pmath_expr_length(item) == 1){
        list = pmath_expr_set_item(list, i, pmath_expr_get_item(item, 1));
      }
      else{
        item = pmath_expr_set_item(item, 0, pmath_ref(head));
        list = pmath_expr_set_item(list, i, item);
        item = NULL;
      }
    }
    
    pmath_unref(item);
  }
  
  if(pmath_expr_length(list) <= 1)
    return list;
  
  cmp_expr = pmath_expr_new(pmath_ref(cmp), 2);
  for(i = 1;i < len;++i){
    pmath_t item = pmath_expr_get_item(list, i);
  
    if(item == PMATH_UNDEFINED)
      continue;
    
    if(pmath_is_expr_of(item, head)){
      pmath_unref(item);
      continue;
    }
    
    cmp_expr = pmath_expr_set_item(cmp_expr, 1, item);
    for(j = i+1;j <= len;++j){
      item = pmath_expr_get_item(list, j);
      
      if(item == PMATH_UNDEFINED)
        continue;
      
      if(pmath_is_expr_of(item, head)){
        pmath_unref(item);
        continue;
      }
      
      cmp_expr = pmath_expr_set_item(cmp_expr, 2, item);
      test = pmath_evaluate(pmath_ref(cmp_expr));
      pmath_unref(test);
      
      if(test == PMATH_SYMBOL_FALSE){
        list = pmath_expr_set_item(list, i, PMATH_UNDEFINED);
        break;
      }
      
      if(test == PMATH_SYMBOL_TRUE){
        list = pmath_expr_set_item(list, j, PMATH_UNDEFINED);
      }
    }
  }
  
  pmath_unref(cmp_expr);
  return pmath_expr_remove_all(list, PMATH_UNDEFINED);
}

PMATH_PRIVATE pmath_t builtin_max(pmath_expr_t expr){
  expr = find_best(expr, PMATH_SYMBOL_GREATEREQUAL, PMATH_SYMBOL_MAX);
  
  expr = pmath_expr_flatten(expr, pmath_ref(PMATH_SYMBOL_LIST), SIZE_MAX);
  
  if(pmath_expr_length(expr) == 0){
    pmath_unref(expr);
    return NEG(pmath_ref(_pmath_object_infinity));
  }
  
  if(pmath_expr_length(expr) == 1){
    pmath_t result = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return result;
  }
  
  return expr;
}

PMATH_PRIVATE pmath_t builtin_min(pmath_expr_t expr){
  expr = find_best(expr, PMATH_SYMBOL_LESSEQUAL, PMATH_SYMBOL_MIN);
  
  expr = pmath_expr_flatten(expr, pmath_ref(PMATH_SYMBOL_LIST), SIZE_MAX);
  
  if(pmath_expr_length(expr) == 0){
    pmath_unref(expr);
    return pmath_ref(_pmath_object_infinity);
  }
  
  if(pmath_expr_length(expr) == 1){
    pmath_t result = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return result;
  }
  
  return expr;
}
