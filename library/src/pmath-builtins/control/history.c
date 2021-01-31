#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_DollarLine;
extern pmath_symbol_t pmath_System_Plus;

PMATH_PRIVATE pmath_t builtin_history(pmath_expr_t expr) {
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_integer(obj)) {
    if(pmath_number_sign(obj) <= 0) {
      obj = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(pmath_System_Plus), 2,
                pmath_ref(pmath_System_DollarLine),
                obj));
                
      if( pmath_is_integer(obj) &&
          pmath_number_sign(obj) > 0)
      {
        return pmath_expr_set_item( expr, 1, obj);
      }
      
      pmath_unref(obj);
      pmath_unref(expr);
      return PMATH_NULL;
    }
    
    pmath_unref(obj);
    return expr;
  }
  
  pmath_unref(obj);
  pmath_unref(expr);
  
  return PMATH_NULL;
}
