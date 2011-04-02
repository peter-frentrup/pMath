#include <pmath-language/patterns-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>

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
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_catch(pmath_expr_t expr){
/* Catch(expr, rule1, rule2, ...)
   Catch(expr, pattern, ...)  == Catch(expr, x:pattern:>x, ...)
 */
  pmath_t result, exception;
  size_t len = pmath_expr_length(expr);
  size_t i;

  if(len < 2){
    pmath_message_argxxx(len, 2, SIZE_MAX);
    return expr;
  }

  result = pmath_evaluate(pmath_expr_get_item(expr, 1));

  exception = pmath_catch();
  if(pmath_is_evaluatable(exception)){
    pmath_unref(result);
    
    for(i = 2;i <= len;++i){
      pmath_t rule = pmath_expr_get_item(expr, i);
      
      if(_pmath_is_rule(rule)){
        pmath_t pattern = pmath_expr_get_item(rule, 1);
        pmath_t rhs     = pmath_expr_get_item(rule, 2);
        
        if(_pmath_pattern_match(exception, pattern, &rhs)){
          pmath_debug_print_object("[caught ", exception, "]\n");
      
          pmath_unref(exception);
          pmath_unref(rule);
          pmath_unref(expr);
          return rhs;
        }
        
        pmath_unref(rhs);
      }
      else{
        pmath_t pattern = pmath_ref(rule);
        pmath_t rhs     = PMATH_NULL;
        
        if(_pmath_pattern_match(exception, pattern, &rhs)){
          pmath_debug_print_object("[caught ", exception, "]\n");
          
          pmath_unref(rule);
          pmath_unref(expr);
          return exception;
        }
      }
      
      pmath_unref(rule);
    }
    
    pmath_unref(expr);
    pmath_throw(exception);
    return PMATH_NULL;
  }
  else{
    if(!pmath_same(exception, PMATH_UNDEFINED))
      pmath_debug_print_object("[uncatchable ", exception, "]\n");
      
    pmath_throw(exception);
  }
  
  pmath_unref(expr);
  return result;
}
