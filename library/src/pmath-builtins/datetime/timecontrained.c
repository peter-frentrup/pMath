#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_timeconstrained(pmath_expr_t expr) {
  /** TimeConstrained(expr, t)            =  TimeConstrained(expr, t, $Failed)
      TimeConstrained(expr, t, failexpr)
   */
  size_t len = pmath_expr_length(expr);
  pmath_t obj;
  pmath_thread_t current_thread;
  
  double seconds;
  pmath_symbol_t guard;
  pmath_t ex;
  pmath_messages_t mq;
  
  if(len < 2 || len > 3) {
    pmath_message_argxxx(len, 2, 3);
    return expr;
  }
  
  obj = pmath_evaluate(pmath_expr_get_item(expr, 2));
  if(pmath_equals(obj, _pmath_object_pos_infinity)) {
    pmath_unref(obj);
    obj = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return obj;
  }
  
  seconds = 0.0;
  
  obj = pmath_approximate(obj, -HUGE_VAL, -HUGE_VAL, NULL);
  if(pmath_is_number(obj)) {
    seconds = pmath_number_get_d(obj);
  }
  
  if(seconds <= 0) {
    pmath_message(PMATH_NULL, "timc", 1, obj);
    return expr;
  }
  
  pmath_unref(obj);
  
  current_thread = pmath_thread_get_current();
  if(!current_thread) {
    pmath_unref(expr);
    return PMATH_NULL;
  }
  
  guard = pmath_symbol_create_temporary(
            PMATH_C_STRING("System`TimeConstrained`stop"), TRUE);
            
  // guard:= Throw(Unevaluated(guard))
  pmath_symbol_set_value(guard,
                         pmath_expr_new_extended(
                           pmath_ref(PMATH_SYMBOL_THROW), 1,
                           pmath_expr_new_extended(
                             pmath_ref(PMATH_SYMBOL_UNEVALUATED), 1,
                             pmath_ref(guard))));
                             
  mq = pmath_ref(current_thread->message_queue);// = pmath_thread_get_queue();
  pmath_thread_send_delayed(mq, pmath_ref(guard), seconds);
  pmath_unref(mq);
  
  obj = pmath_evaluate(pmath_expr_get_item(expr, 1));
  
  // guard:=.    to ignore later time-out event
  pmath_symbol_set_value(guard, PMATH_UNDEFINED);
  
  ex = _pmath_thread_catch(current_thread);
  if(pmath_same(ex, guard)) { // time-out
    pmath_unref(ex);
    pmath_unref(obj);
    if(len == 3)
      obj = pmath_expr_get_item(expr, 3);
    else
      obj = pmath_ref(PMATH_SYMBOL_ABORTED);
  }
  else if(!pmath_same(ex, PMATH_UNDEFINED)) // other error
    _pmath_thread_throw(current_thread, ex);
    
  pmath_unref(guard);
  pmath_unref(expr);
  return obj;
}
