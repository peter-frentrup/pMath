#include <pmath-language/patterns-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


static pmath_bool_t do_with(
  pmath_t *body, 
  pmath_t lhs,       // will be freed
  pmath_t value,     // will be freed
  pmath_t symbols,   // won't be freed
  pmath_t assignment // won't be freed
) {
  if(pmath_is_symbol(lhs)) {
    *body = _pmath_replace_local(*body, lhs, value);
    pmath_unref(lhs);
    pmath_unref(value);
    return TRUE;
  }
  
  if(pmath_is_expr_of(lhs, PMATH_SYMBOL_LIST)) {
    size_t len = pmath_expr_length(lhs);
    size_t i;
    
    if(!pmath_is_expr_of_len(value, PMATH_SYMBOL_LIST, len)) {
      pmath_message(PMATH_NULL, "incomp", 2, lhs, value);
      return FALSE;
    }
    
    for(i = len;i > 0;--i) {
      pmath_t lhs_i = pmath_expr_get_item(lhs, i);
      pmath_t value_i = pmath_expr_get_item(value, i);
      
      if(!do_with(body, lhs_i, value_i, symbols, assignment)) {
        pmath_unref(lhs);
        pmath_unref(value);
        return FALSE;
      }
    }
    
    pmath_unref(lhs);
    pmath_unref(value);
    return TRUE;
  }
  
  pmath_unref(lhs);
  pmath_unref(value);
  if(pmath_same(lhs, PMATH_NULL))
    return TRUE;
  
  pmath_message(PMATH_NULL, "nodef", 2, pmath_ref(symbols), pmath_ref(assignment));
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_with(pmath_expr_t expr) {
  pmath_expr_t symbols;
  pmath_t      body;
  size_t i, len;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  symbols = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(symbols, PMATH_SYMBOL_LIST)) {
    pmath_message(PMATH_NULL, "nolist", 1, symbols);
    return expr;
  }
  
  body = pmath_expr_get_item(expr, 2);
  len  = pmath_expr_length(symbols);
  if(len == 0) {
    pmath_unref(symbols);
    pmath_unref(expr);
    return body;
  }
  
  for(i = 1; i <= len; ++i) {
    pmath_t defi = pmath_expr_get_item(symbols, i);
    
    if(pmath_is_expr_of_len(defi, PMATH_SYMBOL_ASSIGN, 2)) {
      pmath_t lhs = pmath_expr_get_item(defi, 1);
      pmath_t value = pmath_evaluate(pmath_expr_get_item(defi, 2));
      if(!do_with(&body, lhs, value, symbols, defi)) {
        pmath_unref(body);
        pmath_unref(symbols);
        pmath_unref(defi);
        return expr;
      }
    }
    else if(pmath_is_expr_of_len(defi, PMATH_SYMBOL_ASSIGNDELAYED, 2)) {
      pmath_t lhs = pmath_expr_get_item(defi, 1);
      pmath_t value = pmath_expr_get_item(defi, 2);
      if(!do_with(&body, lhs, value, symbols, defi)) {
        pmath_unref(body);
        pmath_unref(symbols);
        pmath_unref(defi);
        return expr;
      }
    }
    
    pmath_unref(defi);
  }
  
  pmath_unref(expr);
  pmath_unref(symbols);
  return body;
}
