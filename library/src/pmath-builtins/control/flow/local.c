#include <pmath-language/patterns-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_Assign;
extern pmath_symbol_t pmath_System_AssignDelayed;
extern pmath_symbol_t pmath_System_List;

static pmath_bool_t make_local(
  pmath_t *body,
  pmath_t *sym 
) {
  if(pmath_is_symbol(*sym)) {
    pmath_t newsym = pmath_symbol_create_temporary(pmath_symbol_name(*sym), TRUE);
    
    *body = _pmath_replace_local(*body, *sym, newsym);
    
    pmath_unref(*sym);
    *sym = newsym;
    return TRUE;
  }
  
  if(pmath_is_expr_of(*sym, pmath_System_List)) {
    size_t i;
    for(i = pmath_expr_length(*sym); i > 0; --i) {
      pmath_t sym_i = pmath_expr_get_item(*sym, i);
      if(make_local(body, &sym_i)) {
        *sym = pmath_expr_set_item(*sym, i, sym_i);
      }
      else {
        pmath_unref(sym_i);
        return FALSE;
      }
    }
    return TRUE;
  }
  
  if(pmath_same(*sym, PMATH_NULL))
    return TRUE;
  
  return FALSE;
}


PMATH_PRIVATE pmath_t builtin_local(pmath_expr_t expr) {
  pmath_expr_t symbols;
  pmath_t      body;
  size_t i, len;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  symbols = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(symbols, pmath_System_List)) {
    pmath_message(PMATH_NULL, "lvlist", 1, symbols);
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
    
    if(pmath_is_expr(defi)) {
      pmath_t sym = pmath_expr_get_item(defi, 0);
      pmath_unref(sym);
      
      if( (!pmath_same(sym, pmath_System_Assign) && !pmath_same(sym, pmath_System_AssignDelayed)) ||
          pmath_expr_length(defi) != 2)
      {
        pmath_unref(body);
        pmath_message(PMATH_NULL, "nodef", 2, symbols, defi);
        return expr;
      }
      
      sym = pmath_expr_get_item(defi, 1);
      if(make_local(&body, &sym)) {
        defi = pmath_expr_set_item(defi, 1, sym);
        symbols = pmath_expr_set_item(symbols, i, defi);
      }
      else {
        pmath_unref(body);
        pmath_unref(sym);
        pmath_message(PMATH_NULL, "nodef", 2, symbols, defi);
        return expr;
      }
    }
    else if(make_local(&body, &defi)) {
      symbols = pmath_expr_set_item(symbols, i, defi);
    }
    else {
      pmath_unref(body);
      pmath_message(PMATH_NULL, "nodef", 2, symbols, defi);
      return expr;
    }
  }
  
  pmath_unref(expr);
  pmath_unref(pmath_evaluate(symbols));
  
  return body;
}
