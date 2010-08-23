#include <pmath-core/expressions.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/messages.h>
#include <pmath-util/modules-private.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_loadlibrary(pmath_expr_t expr){
  pmath_t filename;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
  }
  
  filename = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(!pmath_instance_of(filename, PMATH_TYPE_STRING)
  || pmath_string_length(filename) == 0){
    pmath_message(NULL, "fstr", 1, filename);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  if(!_pmath_module_load(pmath_ref(filename))){
    pmath_message(NULL, "fail", 1, filename);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(filename);
  return NULL;
}
