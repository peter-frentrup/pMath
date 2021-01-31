#include <pmath-core/numbers.h>

#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_Throw;
extern pmath_symbol_t pmath_System_Unevaluated;

PMATH_PRIVATE pmath_t builtin_pause(pmath_expr_t expr) {
  pmath_t  arg;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  arg = pmath_expr_get_item(expr, 1);
  
  if(!pmath_is_number(arg) || pmath_number_sign(arg) < 0) {
    pmath_message(
      PMATH_NULL, "numn", 2,
      PMATH_FROM_INT32(1),
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
      return PMATH_NULL;
      
    guard = pmath_symbol_create_temporary(
              PMATH_C_STRING("System`Pause`stop"), TRUE);
              
    // guard:= Throw(Unevaluated(guard))
    pmath_symbol_set_value(guard,
                           pmath_expr_new_extended(
                             pmath_ref(pmath_System_Throw), 1,
                             pmath_expr_new_extended(
                               pmath_ref(pmath_System_Unevaluated), 1,
                               pmath_ref(guard))));
                               
    mq = pmath_ref(current_thread->message_queue);// = pmath_thread_get_queue();
    pmath_thread_send_delayed(mq, pmath_ref(guard), time);
    pmath_unref(mq);
    
    
    while(!pmath_thread_aborting(current_thread)) {
      pmath_thread_sleep();
    }
    
    
    // guard:=.    to ignore later time-out event
    pmath_symbol_set_value(guard, PMATH_UNDEFINED);
    
    arg = _pmath_thread_catch(current_thread);
    if(pmath_same(arg, guard)) { // time-out
      pmath_unref(arg);
    }
    else if(!pmath_same(arg, PMATH_UNDEFINED)) // other error
      _pmath_thread_throw(current_thread, arg);
      
    pmath_unref(guard);
  }
  
  return PMATH_NULL;
}
