#include <pmath-builtins/all-symbols-private.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/messages.h>

static void protect_callback(pmath_symbol_t symbol){
  pmath_symbol_attributes_t attr = pmath_symbol_get_attributes(symbol);
  
  if(!(attr & PMATH_SYMBOL_ATTRIBUTE_PROTECTED))
    pmath_emit(pmath_symbol_name(symbol), NULL);
  
  pmath_symbol_set_attributes(symbol, attr | PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
}

static void unprotect_callback(pmath_symbol_t symbol){
  pmath_symbol_attributes_t attr = pmath_symbol_get_attributes(symbol);
  
  if(attr & PMATH_SYMBOL_ATTRIBUTE_PROTECTED)
    pmath_emit(pmath_symbol_name(symbol), NULL);
  
  pmath_symbol_set_attributes(symbol, attr & ~PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
}

PMATH_PRIVATE pmath_t builtin_protect_or_unprotect(pmath_expr_t expr){
  pmath_t          head;
  pmath_callback_t callback;
  size_t i;
  
  for(i = 1;i <= pmath_expr_length(expr);++i){
    pmath_symbol_t sym = pmath_expr_get_item(expr, i);
    
    if(pmath_instance_of(sym, PMATH_TYPE_STRING)){
      sym = pmath_symbol_get(sym, FALSE);
      if(sym)
        expr = pmath_expr_set_item(expr, i, pmath_ref(sym));
      else
        sym = pmath_expr_get_item(expr, i);
    }
    
    if(!pmath_instance_of(sym, PMATH_TYPE_SYMBOL)){
      pmath_message(NULL, "fnsym", 1, sym);
      return expr;
    }
    
    pmath_unref(sym);
  }

  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);

  callback = (pmath_callback_t)protect_callback;
  if(head == PMATH_SYMBOL_UNPROTECT)
    callback = (pmath_callback_t)unprotect_callback;
  
  pmath_gather_begin(NULL);
  
  for(i = 1;i <= pmath_expr_length(expr);++i){
    pmath_symbol_t symbol = (pmath_symbol_t)pmath_expr_get_item(expr, i);

    pmath_symbol_synchronized(symbol, callback, symbol);
    pmath_unref(symbol);
  }

  pmath_unref(expr);
  return pmath_gather_end();
}
