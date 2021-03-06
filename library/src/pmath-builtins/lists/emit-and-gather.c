#include <pmath-core/expressions-private.h>

#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_List;

PMATH_PRIVATE pmath_t builtin_gather(pmath_expr_t expr) {
  /* Gather(expr, pattern)
     Gather(expr)           = Gather(expr, ~)
   */
  pmath_t eval, list;
  size_t len = pmath_expr_length(expr);
  
  if(len < 1 || len > 2) {
    pmath_message_argxxx(len, 1, 2);
    return expr;
  }
  
  if(len > 1)
    pmath_gather_begin(pmath_expr_get_item(expr, 2));
  else
    pmath_gather_begin(pmath_ref(_pmath_object_singlematch));
    
  eval = pmath_evaluate(pmath_expr_get_item(expr, 1));
  
  list = pmath_gather_end();
  
  pmath_unref(expr);
  return pmath_expr_new_extended(
           pmath_ref(pmath_System_List), 2, eval, list);
}

PMATH_PRIVATE pmath_t builtin_regather(pmath_expr_t expr) {
  /* ReGather(pattern)
     ReGather()           = ReGather(~)
   */
  pmath_expr_t result;
  size_t len = pmath_expr_length(expr);
  if(len > 1) {
    pmath_message_argxxx(len, 0, 1);
    return expr;
  }
  
  result = pmath_gather_end();
  if(pmath_is_null(result) && !pmath_aborting()) {
    pmath_unref(expr);
    pmath_message(PMATH_NULL, "nogather", 0);
    return pmath_ref(_pmath_object_emptylist);
  }
  
  if(len == 1)
    pmath_gather_begin(pmath_expr_get_item(expr, 1));
  else
    pmath_gather_begin(pmath_ref(_pmath_object_singlematch));
    
  pmath_unref(expr);
  return result;
}

PMATH_PRIVATE pmath_t builtin_emit(pmath_expr_t expr) {
  /* Emit(obj, tag)
     Emit(obj)       = Emit(obj, /\/)
   */
  pmath_t obj, tag;
  size_t len = pmath_expr_length(expr);
  
  if(len < 1 || len > 2) {
    pmath_message_argxxx(len, 1, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  tag = PMATH_NULL;
  if(len == 2)
    tag = pmath_expr_get_item(expr, 2);
    
  pmath_unref(expr);
  pmath_emit(pmath_ref(obj), tag);
  return obj;
}
