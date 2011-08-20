#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_refresh(pmath_expr_t expr) {
  size_t exprlen = pmath_expr_length(expr);
  pmath_thread_t thread = pmath_thread_get_current();
  pmath_t body, options, opt;
  
  if(!thread)
    return expr;
    
  if(exprlen < 2) {
    pmath_message_argxxx(exprlen, 2, 2);
    return expr;
  }
  
  body = pmath_expr_get_item(expr, 1);
  opt = pmath_expr_get_item(expr, 2);
  pmath_unref(opt);
  
  if(pmath_same(opt, PMATH_SYMBOL_NONE)) {
    options = pmath_options_extract(expr, 2);
  }
  else {
    options = pmath_options_extract(expr, 1);
    
    if(!pmath_is_null(options)) {
      opt = pmath_evaluate(pmath_option_value(
                             PMATH_NULL, PMATH_SYMBOL_TRACKEDSYMBOLS, options));
      pmath_unref(opt);
    }
  }
  
  if(pmath_is_null(options)) {
    pmath_unref(body);
    return expr;
  }
  
  pmath_unref(expr);
  
  if(pmath_same(opt, PMATH_SYMBOL_AUTOMATIC)) {
    body = pmath_evaluate(body);
  }
  else {
    intptr_t id = thread->current_dynamic_id;
    thread->current_dynamic_id = 0;
    
    body = pmath_evaluate(body);
    
    thread->current_dynamic_id = id;
  }
  
  if(thread->current_dynamic_id != 0) {
    opt = pmath_evaluate(pmath_option_value(
                           PMATH_NULL, PMATH_SYMBOL_UPDATEINTERVAL, options));
                           
    if(!pmath_same(opt, _pmath_object_infinity)) {
      pmath_t sec_obj = pmath_approximate(pmath_ref(opt), -HUGE_VAL, -HUGE_VAL, NULL);
      double seconds = 0.0;
      
      if(pmath_is_number(sec_obj))
        seconds = pmath_number_get_d(sec_obj);
        
      pmath_unref(sec_obj);
      if(seconds > 0) {
        pmath_thread_send_delayed(
          thread->message_queue,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_INTERNAL_DYNAMICUPDATED), 1,
            pmath_integer_new_siptr(thread->current_dynamic_id)),
          seconds);
      }
      else {
        pmath_message(PMATH_NULL, "timec", 1, pmath_ref(opt));
      }
    }
    
    pmath_unref(opt);
  }
  
  pmath_unref(options);
  return body;
}
