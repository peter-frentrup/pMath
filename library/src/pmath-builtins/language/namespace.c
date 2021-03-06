#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_DollarNamespace;

PMATH_PRIVATE pmath_t builtin_namespace(pmath_expr_t expr) {
  /* Namespace()
     Namespace(sym)
   */
  pmath_t sym;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen == 0) {
    pmath_unref(expr);
    return pmath_ref(pmath_System_DollarNamespace);
  }
  
  if(exprlen != 1) {
    pmath_message_argxxx(exprlen, 0, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  if(pmath_is_symbol(sym)) {
    pmath_string_t name = pmath_symbol_name(sym);
    const uint16_t *buf = pmath_string_buffer(&name);
    int             len = pmath_string_length(name);
    
    pmath_unref(expr);
    pmath_unref(sym);
    
    --len;
    while(len > 0 && buf[len] != '`')
      --len;
      
    if(len > 0)
      return pmath_string_part(name, 0, len + 1);
      
    pmath_unref(name);
    return PMATH_C_STRING("");
  }
  
  pmath_message(PMATH_NULL, "sym", 2, sym, PMATH_FROM_INT32(1));
  return expr;
}
