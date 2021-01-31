#include <pmath-util/messages.h>
#include <pmath-util/modules-private.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_DollarFailed;

PMATH_PRIVATE pmath_t builtin_loadlibrary(pmath_expr_t expr){
/* LoadLibrary(filename)
 */
  pmath_t filename;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
  }
  
  filename = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(!pmath_is_string(filename)
  || pmath_string_length(filename) == 0){
    pmath_message(PMATH_NULL, "fstr", 1, filename);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  if(!_pmath_module_load(pmath_ref(filename))){
    pmath_message(PMATH_NULL, "fail", 1, filename);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  pmath_unref(filename);
  return PMATH_NULL;
}
