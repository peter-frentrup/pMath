#include <pmath-core/numbers.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/dynamic-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>

#include <math.h>


PMATH_PRIVATE pmath_t builtin_clock(pmath_expr_t expr){
  size_t exprlen = pmath_expr_length(expr);
  pmath_thread_t thread = pmath_thread_get_current();
  pmath_t obj;
  pmath_t duration, min, max, delta;
  double now;
  double duration_seconds;
  size_t steps;
  pmath_bool_t backwards;
  size_t max_cycles;
  
  if(!thread)
    return expr;
  
  if(thread->current_dynamic_id != 0){
    now = _pmath_dynamic_first_eval(thread->current_dynamic_id);
    now = -now + pmath_tickcount();
  }
  else
    now = 0;
  
  if(exprlen == 0){
    pmath_unref(expr);
    
    obj = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_INTERNAL_DYNAMICUPDATED), 1,
      pmath_integer_new_si((long)thread->current_dynamic_id));
    pmath_unref(pmath_evaluate(obj));
  
    return pmath_float_new_d(now - floor(now));
  }
  
  if(exprlen > 3){
    pmath_message_argxxx(exprlen, 0, 3);
    return expr;
  }
  
  max_cycles = SIZE_MAX;
  if(exprlen >= 3){
    obj = pmath_expr_get_item(expr, 3);
    
    if(pmath_instance_of(obj, PMATH_TYPE_INTEGER)
    && pmath_integer_fits_ui(obj)){
      max_cycles = pmath_integer_get_ui(obj);
    }
    else if(!(_pmath_number_class(obj) & PMATH_CLASS_POSINF)){
      pmath_unref(obj);
      pmath_message(NULL, "innf", 2, INT(3), pmath_ref(expr));
      return expr;
    }
    
    pmath_unref(obj);
  }
  
  duration = PMATH_UNDEFINED;
  if(exprlen >= 2){
    duration = pmath_expr_get_item(expr, 2);
  }
  
  obj = pmath_expr_get_item(expr, 1);
  if(pmath_is_expr_of(obj, PMATH_SYMBOL_RANGE)){
    size_t objlen = pmath_expr_length(obj);
    
    if(objlen == 2){
      min = pmath_expr_get_item(obj, 1);
      max = pmath_expr_get_item(obj, 2);
      delta = pmath_integer_new_si(0);
    }
    else if(objlen == 3){
      min   = pmath_expr_get_item(obj, 1);
      max   = pmath_expr_get_item(obj, 2);
      delta = pmath_expr_get_item(obj, 3);
    }
    else{
      pmath_unref(duration);
      pmath_message(NULL, "vals", 1, obj);
      return expr;
    }
    
    if(!min || !max || !delta){
      pmath_unref(duration);
      pmath_unref(min);
      pmath_unref(max);
      pmath_unref(delta);
      pmath_message(NULL, "vals", 1, obj);
      return expr;
    }
    
    pmath_unref(obj);
  }
  else{
    min   = INT(0);
    max   = obj;
    delta = pmath_integer_new_si(0);
  }
  
  backwards = FALSE;
  obj = pmath_evaluate(GREATER(pmath_ref(min), pmath_ref(max)));
  pmath_unref(obj);
  if(obj == PMATH_SYMBOL_TRUE){
    backwards = TRUE;
    
    obj = min;
    min = max;
    max = obj;
    delta = NEG(delta);
  }
  
  obj = pmath_approximate(pmath_ref(delta), -HUGE_VAL, -HUGE_VAL);
  if(!pmath_instance_of(obj, PMATH_TYPE_NUMBER)
  || pmath_number_sign(obj) < 0){
    pmath_unref(min);
    pmath_unref(max);
    pmath_unref(delta);
    pmath_unref(obj);
    pmath_unref(duration);
    pmath_message(NULL, "length", 1, pmath_ref(expr));
    return expr;
  }
  
  if(pmath_number_sign(obj) == 0){
    steps = 0;
    pmath_unref(obj);
    
    if(_pmath_number_class(max) & PMATH_CLASS_POSINF){
      steps = SIZE_MAX;
      pmath_unref(delta);
      delta = pmath_integer_new_si(1);
      
      if(duration != PMATH_UNDEFINED){
        pmath_unref(min);
        pmath_unref(max);
        pmath_unref(delta);
        pmath_unref(duration);
        pmath_message(NULL, "inf", 0);
        return expr;
      }
    }
  }
  else{
    obj = PLUS(INT(1), 
      FUNC(pmath_ref(PMATH_SYMBOL_FLOOR), 
        DIV(MINUS(pmath_ref(max), pmath_ref(min)), obj)));
    
    obj = pmath_evaluate(obj);
    
    if(pmath_instance_of(obj, PMATH_TYPE_INTEGER)
    && pmath_integer_fits_ui(obj)){
      steps = pmath_integer_get_ui(obj);
      pmath_unref(obj);
    }
    else if(_pmath_number_class(obj) & PMATH_CLASS_POSINF){
      steps = SIZE_MAX;
      pmath_unref(obj);
      
      if(duration != PMATH_UNDEFINED){
        pmath_unref(min);
        pmath_unref(max);
        pmath_unref(delta);
        pmath_unref(duration);
        pmath_message(NULL, "inf", 0);
        return expr;
      }
    }
    else{
      pmath_unref(min);
      pmath_unref(max);
      pmath_unref(delta);
      pmath_unref(obj);
      pmath_unref(duration);
      pmath_message(NULL, "length", 1, pmath_ref(expr));
      return expr;
    }
  }
  
  if(steps == SIZE_MAX){
    duration_seconds = HUGE_VAL;
  }
  else{
    if(duration == PMATH_UNDEFINED){
      if(steps+1 > 1) // neither 0 not SIZE_MAX
        duration = TIMES(pmath_integer_new_si(steps), pmath_ref(delta));
      else
        duration = MINUS(pmath_ref(max), pmath_ref(min));
    }
    
    obj = pmath_approximate(pmath_ref(duration), -HUGE_VAL, -HUGE_VAL);
    if(!pmath_instance_of(obj, PMATH_TYPE_NUMBER)){
      pmath_unref(obj);
      pmath_unref(min);
      pmath_unref(max);
      pmath_unref(delta);
      pmath_message(NULL, "period", 1, duration);
      return expr;
    }
    
    duration_seconds = pmath_number_get_d(obj);
    pmath_unref(obj);
  }
  pmath_unref(duration);
  pmath_unref(expr);
  
  if(backwards){
    obj = min;
    min = max;
    max = obj;
  }
  
  if(duration_seconds < HUGE_VAL){
    now/= duration_seconds;
    
    if(max_cycles != SIZE_MAX && now > max_cycles){
      pmath_unref(min);
      pmath_unref(delta);
      return max;
    }
  
    now = fmod(now, 1.0);
  }
  
  obj = pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_INTERNAL_DYNAMICUPDATED), 1,
    pmath_integer_new_si((long)thread->current_dynamic_id));
  pmath_unref(pmath_evaluate(obj));
  
  if(steps+1 > 1){ // neither 0 nor SIZE_MAX
    steps = (size_t)(floor(now * steps));
    
    pmath_unref(max);
    
    return PLUS(min, TIMES(pmath_integer_new_ui(steps), delta));
  }
  
  if(steps < SIZE_MAX){
    pmath_unref(delta);
    delta = MINUS(max, pmath_ref(min));
  }
  else{
    pmath_unref(max);
  }
  
  return PLUS(min, TIMES(pmath_float_new_d(now), delta));
}
