#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

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
  
  pmath_message(NULL, "sym", 2, sym, pmath_integer_new_si(1));
  return expr;
}
