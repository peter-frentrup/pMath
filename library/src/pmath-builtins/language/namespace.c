#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_namespace(pmath_expr_t expr){
/* Namespace()
   Namespace(sym)
 */
  pmath_t sym;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen == 0){
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_CURRENTNAMESPACE);
  }
  
  if(exprlen != 1){
    pmath_message_argxxx(exprlen, 0, 1);
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
    
    if(len > 0)
      return pmath_string_part(name, 0, len+1);
    
    pmath_unref(name);
    return PMATH_C_STRING("");
  }
  
  pmath_unref(sym);
  pmath_message(NULL, "sym", 2, pmath_integer_new_si(1), pmath_ref(expr));
  return expr;
}
