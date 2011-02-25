#include <pmath-language/patterns-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_local(pmath_expr_t expr){
  pmath_expr_t symbols;
  pmath_t      body;
  size_t i, len;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  symbols = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(symbols, PMATH_SYMBOL_LIST)){
    pmath_message(NULL, "lvlist", 1, symbols);
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
    pmath_symbol_t sym, localsym;
    
    if(pmath_is_expr(defi)){
      sym = pmath_expr_get_item(defi, 0);
      pmath_unref(sym);
      
      if((sym != PMATH_SYMBOL_ASSIGN && sym != PMATH_SYMBOL_ASSIGNDELAYED)
      || pmath_expr_length(defi) != 2){
        pmath_unref(body);
        pmath_message(NULL, "nodef", 2, symbols, defi);
        return expr;
      }
      
      sym = pmath_expr_get_item(defi, 1);
      if(!pmath_is_symbol(sym)){
        pmath_unref(body);
        pmath_unref(sym);
        pmath_message(NULL, "nodef", 2, symbols, defi);
        return expr;
      }
    }
    else if(pmath_is_symbol(defi)){
      sym = pmath_ref(defi);
    }
    else{
      pmath_unref(body);
      pmath_message(NULL, "nodef", 2, symbols, defi);
      return expr;
    }
    
    localsym = pmath_symbol_create_temporary(pmath_symbol_name(sym), TRUE);
    
    body = _pmath_replace_local(body, sym, localsym);
    
    pmath_unref(sym);
    
    if(pmath_is_expr(defi)){
      defi = pmath_expr_set_item(defi, 1, localsym);
    }
    else{
      pmath_unref(defi);
      defi = localsym;
    }
    
    symbols = pmath_expr_set_item(symbols, i, defi);
  }
  
  pmath_unref(expr);
  pmath_unref(pmath_evaluate(symbols));
   
  return body;
}
