#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <math.h>
#include <string.h>

#include <pmath-core/custom.h>

#include <pmath-util/hashtables-private.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/concurrency/threads-private.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_pause(pmath_expr_t expr){
  pmath_t  arg;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  arg = pmath_expr_get_item(expr, 1);
  
  if(!pmath_instance_of(arg, PMATH_TYPE_NUMBER)
  || pmath_number_sign((pmath_number_t)arg) < 0){
    pmath_message(
      NULL, "numn", 2,
      pmath_integer_new_ui(1),
      pmath_ref(expr));
    
    pmath_unref(arg);
    return expr;
  }
  
  pmath_unref(expr);
  
  {
    pmath_thread_t current_thread = pmath_thread_get_current();
    pmath_messages_t mq;
    pmath_symbol_t guard;
    double time = pmath_number_get_d(arg);
    pmath_unref(arg);
    
    if(!current_thread || time < 0.001)
      return NULL;
      
    guard = pmath_symbol_create_temporary(
      PMATH_C_STRING("System`Pause`stop"), TRUE);
    
    // guard:= Throw(Unevaluated(guard))
    pmath_symbol_set_value(guard, 
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_THROW), 1,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_UNEVALUATED), 1,
          pmath_ref(guard))));
    
    mq = pmath_ref(current_thread->message_queue);// = pmath_thread_get_queue();
    pmath_thread_send_delayed(mq, pmath_ref(guard), time);
    pmath_unref(mq);
    
    
    while(!pmath_thread_aborting(current_thread)){
      pmath_thread_sleep();
    }
    
    
    // guard:=.    to ignore later time-out event
    pmath_symbol_set_value(guard, PMATH_UNDEFINED);
    
    arg = _pmath_thread_catch(current_thread);
    if(arg == guard){ // time-out
      pmath_unref(arg);
    }
    else if(arg != PMATH_UNDEFINED) // other error
      _pmath_thread_throw(current_thread, arg);
    
    pmath_unref(guard);
  }
  
  return NULL;
}
