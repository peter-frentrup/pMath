#include <pmath-util/evaluation.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_history(pmath_expr_t expr){
  pmath_t obj;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  obj = pmath_expr_get_item(expr, 1);
  
  if(pmath_instance_of(obj, PMATH_TYPE_INTEGER)){
    if(pmath_number_sign((pmath_number_t)obj) <= 0){
      obj = pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PLUS), 2,
          pmath_ref(PMATH_SYMBOL_LINE),
          obj));
      
      if(pmath_instance_of(obj, PMATH_TYPE_INTEGER)
      && pmath_number_sign((pmath_number_t)obj) > 0)
        return pmath_expr_set_item( expr, 1, obj);
      
      pmath_unref(obj);
      pmath_unref(expr);
      return NULL;
    }
    
    pmath_unref(obj);
    return expr;
  }
  
  pmath_unref(obj);
  pmath_unref(expr);
  
  return NULL;
}
