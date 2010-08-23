#include <pmath-core/expressions.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_update(pmath_expr_t expr){
  pmath_t  sym;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  sym = pmath_expr_get_item(expr, 1);
  if(pmath_instance_of(sym, PMATH_TYPE_STRING)){
    sym = pmath_symbol_find(sym, FALSE);
  }
  
  if(!pmath_instance_of(sym, PMATH_TYPE_SYMBOL)){
    pmath_message(NULL, "ssym", 1, sym);
    return expr;
  }
  
  pmath_unref(expr);
  
  pmath_symbol_update(sym);
  pmath_unref(sym);
  pmath_unref(expr);
  return NULL;
}
