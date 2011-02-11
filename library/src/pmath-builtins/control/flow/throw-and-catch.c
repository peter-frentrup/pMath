#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_throw(pmath_expr_t expr){
/* Throw(expr)
 */
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
//  #ifdef PMATH_DEBUG_LOG
//  {
//    pmath_t stack = pmath_evaluate(pmath_expr_new(pmath_ref(PMATH_SYMBOL_STACK), 0));
//    
//    pmath_debug_print_object("[", expr, "");
//    pmath_debug_print_object(" during ", stack, "]\n");
//    
//    pmath_unref(stack);
//  }
//  #endif
  
  pmath_throw(pmath_expr_get_item(expr, 1));
  pmath_unref(expr);
  return NULL;
}

PMATH_PRIVATE pmath_t builtin_catch(pmath_expr_t expr){
/* Catch(expr, pattern)  = Catch(expr, pattern)
   Catch(expr)           = Catch(expr, ~)
 */
  pmath_t result, exception;
  size_t len = pmath_expr_length(expr);

  if(len < 1 || len > 2){
    pmath_message_argxxx(len, 1, 2);
    return expr;
  }

  result = pmath_evaluate(pmath_expr_get_item(expr, 1));

  exception = pmath_catch();
  if(!exception || pmath_instance_of(exception, PMATH_TYPE_EVALUATABLE)){
    pmath_t rhs, pattern;

    if(len == 1){
      pmath_unref(result);
      pmath_unref(expr);
      return exception;
    }

    rhs = NULL;
    pattern = pmath_expr_get_item(expr, 2);
    if(_pmath_pattern_match(exception, pattern, &rhs)){
      pmath_unref(result);
      
      pmath_debug_print_object("[caught ", exception, "]\n");
      
      pmath_unref(expr);
      return exception;
    }

    pmath_throw(exception); 
  }
  else{
    if(exception != PMATH_UNDEFINED)
      pmath_debug_print_object("[uncatchable ", exception, "]\n");
      
    pmath_unref(exception);
  }
  
  pmath_unref(expr);
  return result;
}
