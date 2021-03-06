#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_Names;

PMATH_PRIVATE pmath_t builtin_remove(pmath_expr_t expr) {
  /* Remove("symbol1", "symbol2", ...)
   */
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen == 0) {
    pmath_message_argxxx(exprlen, 1, SIZE_MAX);
    return expr;
  }
  
  for(i = 1; i <= exprlen; ++i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    expr = pmath_expr_set_item(expr, i, PMATH_NULL);
    
    if(pmath_is_symbol(item)) {
      pmath_symbol_remove(item);
    }
    else if(pmath_is_string(item)) {
      item = pmath_evaluate(
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_Names), 1,
                 item));
                 
      if(pmath_is_expr_of(item, pmath_System_List)) {
        size_t j;
        for(j = pmath_expr_length(item); j > 0; --j) {
          pmath_t name = pmath_expr_get_item(item, j);
          
          if(pmath_is_string(name)) {
            name = pmath_symbol_get(name, FALSE);
            if(!pmath_is_null(name))
              pmath_symbol_remove(name);
          }
          else
            pmath_unref(name);
        }
      }
      
      pmath_unref(item);
    }
    else {
      pmath_message(PMATH_NULL, "ssym", 1, item);
    }
  }
  
//  for(i = pmath_expr_length(expr);i > 0;--i){
//    item = pmath_expr_get_item(expr, i);
//    expr = pmath_expr_set_item(expr, i, PMATH_NULL);
//
//    if(pmath_is_symbol(item)){
//      sym = item;
//    }
//    else if(pmath_is_string(item)){
//      sym = pmath_symbol_find(pmath_ref(item), FALSE);
//
//      if(!sym){
//        pmath_message(PMATH_NULL, "notfound", 1, item);
//        continue;
//      }
//
//      pmath_unref(item);
//    }
//    else{
//      sym = PMATH_NULL;
//      pmath_message(PMATH_NULL, "ssym", 1, item);
//      continue;
//    }
//
//    pmath_symbol_remove(sym);
//  }

  pmath_unref(expr);
  return PMATH_NULL;
}
