#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/lists-private.h>

PMATH_PRIVATE pmath_t builtin_symbolname(pmath_expr_t expr){
  pmath_t sym;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  if(pmath_instance_of(sym, PMATH_TYPE_SYMBOL)){
    pmath_string_t name = pmath_symbol_name(sym);
    const uint16_t *buf = pmath_string_buffer(name);
    int             len = pmath_string_length(name);
    
    pmath_unref(expr);
    pmath_unref(sym);
    
    --len;
    while(len > 0 && buf[len] != '`')
      --len;
    
    return pmath_string_part(name, len + 1, -1);
  }
  
  pmath_unref(sym);
  pmath_message(NULL, "sym", 2, pmath_integer_new_si(1), pmath_ref(expr));
  return expr;
}
