#include <pmath-util/evaluation.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-language/patterns-private.h>

PMATH_PRIVATE pmath_t builtin_with(pmath_expr_t expr){
  pmath_expr_t symbols;
  pmath_t      body;
  size_t i, len;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  symbols = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(symbols, PMATH_SYMBOL_LIST)){
    pmath_message(NULL, "nolist", 1, symbols);
    return expr;
  }
  
  body = pmath_expr_get_item(expr, 2);
  len  = pmath_expr_length(symbols);
  if(len == 0){
    pmath_unref(symbols);
    pmath_unref(expr);
    return body;
  }
  
  for(i = 1;i <= len;++i){
    pmath_t defi = pmath_expr_get_item(symbols, i);
    
    if(pmath_is_expr_of_len(defi, PMATH_SYMBOL_ASSIGN, 2)){
      pmath_t sym = pmath_expr_get_item(defi, 1);
      if(pmath_instance_of(sym, PMATH_TYPE_SYMBOL)){
        pmath_t value = pmath_evaluate(pmath_expr_get_item(defi, 2));
        
        pmath_unref(defi);
        body = _pmath_replace_local(body, sym, value);
        
        pmath_unref(sym);
        pmath_unref(value);
        continue;
      }
      pmath_unref(sym);
    }
    
    if(pmath_is_expr_of_len(defi, PMATH_SYMBOL_ASSIGNDELAYED, 2)){
      pmath_t sym = pmath_expr_get_item(defi, 1);
      if(pmath_instance_of(sym, PMATH_TYPE_SYMBOL)){
        pmath_t value = pmath_expr_get_item(defi, 2);
        
        pmath_unref(defi);
        body = _pmath_replace_local(body, sym, value);
        
        pmath_unref(sym);
        pmath_unref(value);
        continue;
      }
      pmath_unref(sym);
    }
    
    pmath_unref(body);
    pmath_message(NULL, "nodef", 2, symbols, defi);
    return expr;
  }
  
  pmath_unref(expr);
  pmath_unref(symbols);
  
  return body;
}
