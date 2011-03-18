#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_bool_t _pmath_run(pmath_t *in_out){
  *in_out = pmath_evaluate(*in_out);
  if(pmath_is_expr(*in_out)){
    pmath_t head = pmath_expr_get_item(*in_out, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_SYMBOL_CONTINUE) 
    || pmath_same(head, PMATH_SYMBOL_BREAK)){
      pmath_bool_t do_break = pmath_same(head, PMATH_SYMBOL_BREAK);
      
      pmath_t counter = pmath_expr_get_item(*in_out, 1);
      if(pmath_is_integer(counter)
      && pmath_compare(counter, PMATH_FROM_INT32(1)) > 0){
        do_break = TRUE;
        counter = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PLUS), 2,
          counter,
          PMATH_FROM_INT32(-1));
        *in_out = pmath_expr_set_item(*in_out, 1, counter);
      }
      else{
        pmath_unref(counter);
        pmath_unref(*in_out);
        *in_out = PMATH_NULL;
      }
      
      return do_break;
    }

    if(pmath_same(head, PMATH_SYMBOL_RETURN) 
    || pmath_same(head, PMATH_SYMBOL_GOTO))
      return TRUE;
  }

  pmath_unref(*in_out);
  *in_out = PMATH_NULL;
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_evaluate(pmath_expr_t expr){
/* Evaluate(expr)
 */
  pmath_t result;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  result = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  return result;
}

PMATH_PRIVATE pmath_t builtin_evaluatedelayed(pmath_expr_t expr){
/* EvaluateDelayed(expr, time)
 */
  pmath_messages_t mq;
  pmath_t reltime_obj;
  double seconds = 0;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  reltime_obj = pmath_expr_get_item(expr, 2);
  reltime_obj = pmath_approximate(reltime_obj, -HUGE_VAL, -HUGE_VAL);
  
  if(pmath_is_number(reltime_obj))
    seconds = pmath_number_get_d(reltime_obj);
  
  pmath_unref(reltime_obj);
  if(seconds <= 0){
    pmath_message(PMATH_NULL, "invtim", 1, pmath_expr_get_item(expr, 2));
    return expr;
  }
  
  mq = pmath_thread_get_queue();
  pmath_thread_send_delayed(mq, pmath_expr_get_item(expr, 1), seconds);
  pmath_unref(mq);
  pmath_unref(expr);
  return PMATH_NULL;
}

  static pmath_t release_hold(pmath_t expr){
    if(pmath_is_expr(expr)){
      size_t i;
      pmath_bool_t must_flatten;
      pmath_t head = pmath_expr_get_item(expr, 0);
      
      if(pmath_same(head, PMATH_SYMBOL_HOLD)
      || pmath_same(head, PMATH_SYMBOL_HOLDCOMPLETE)
      || pmath_same(head, PMATH_SYMBOL_HOLDFORM)
      || pmath_same(head, PMATH_SYMBOL_HOLDPATTERN)){
        pmath_unref(head);
        return pmath_expr_set_item(expr, 0, PMATH_UNDEFINED);
      }
      
      head = release_hold(head);
      if(pmath_is_expr_of(head, PMATH_UNDEFINED)){
        if(pmath_expr_length(head) == 1){
          pmath_t arg = pmath_expr_get_item(head, 1);
          pmath_unref(head);
          head = arg;
        }
        else
          head = pmath_expr_set_item(head, 0, pmath_ref(PMATH_SYMBOL_SEQUENCE));
      }
      
      must_flatten = FALSE;
      for(i = pmath_expr_length(expr);i > 0;--i){
        pmath_t item = pmath_expr_get_item(expr, i);
        
        item = release_hold(item);
        if(pmath_is_expr_of(item, PMATH_UNDEFINED)){
          if(pmath_expr_length(item) == 1){
            pmath_t arg = pmath_expr_get_item(item, 1);
            pmath_unref(item);
            item = arg;
          }
          else
            must_flatten = TRUE;
        }
        
        expr = pmath_expr_set_item(expr, i, item);
      }
      
      if(must_flatten){
        expr = pmath_expr_set_item(expr, 0, PMATH_UNDEFINED);
        expr = pmath_expr_flatten(expr, PMATH_UNDEFINED, 1);
      }
      
      expr = pmath_expr_set_item(expr, 0, head);
    }
    
    return expr;
  }

PMATH_PRIVATE pmath_t builtin_releasehold(pmath_expr_t expr){
/* ReleaseHold(expr)
 */
  pmath_expr_t result;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  result = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  result = release_hold(result);
  if(pmath_is_expr_of(result, PMATH_UNDEFINED)){
    if(pmath_expr_length(result) == 1){
      pmath_t arg = pmath_expr_get_item(result, 1);
      pmath_unref(result);
      return arg;
    }
    
    return pmath_expr_set_item(result, 0, pmath_ref(PMATH_SYMBOL_SEQUENCE));
  }
  
  return result;
}

PMATH_PRIVATE pmath_t builtin_evaluationsequence(pmath_expr_t expr){
/* expr1; expr2; ...
 */
  pmath_bool_t have_label;
  pmath_t result;
  size_t i;
  size_t len = pmath_expr_length(expr);

  if(len < 1){
    pmath_message_argxxx(len, 1, SIZE_MAX);
    pmath_unref(expr);
    return PMATH_NULL;
  }
  
  have_label = FALSE;
  result = PMATH_NULL;
  
  for(i = 1;i < len || (i==len && have_label);++i){
    pmath_unref(result);
    result = pmath_evaluate(pmath_expr_get_item(expr, i));
    
    if(pmath_is_expr(result)){
      pmath_t head = pmath_expr_get_item(result, 0);
      pmath_unref(head);
      
      if(pmath_same(head, PMATH_SYMBOL_BREAK)
      || pmath_same(head, PMATH_SYMBOL_CONTINUE)
      || pmath_same(head, PMATH_SYMBOL_RETURN)){
        pmath_unref(expr);
        return result;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_LABEL)){
        have_label = TRUE;
      }
      else if(pmath_same(head, PMATH_SYMBOL_GOTO) 
      && pmath_expr_length(result) == 1){
        pmath_t lbl = pmath_expr_get_item(result, 1);
        size_t j;
        
        for(j = have_label ? 1 : i+1;j <= len;++j){
          pmath_t item = pmath_expr_get_item(expr, j);
          
          if(pmath_is_expr_of_len(item, PMATH_SYMBOL_LABEL, 1)){
            head = pmath_expr_get_item(item, 1);
            
            if(pmath_equals(head, lbl)){
              pmath_unref(head);
              pmath_unref(item);
              pmath_unref(lbl);
              result = PMATH_NULL;
              i = j;
              have_label = TRUE; // ++i in outer for loop skips the Label(...)
              
              goto NEXT_STEP;
            }
            
            pmath_unref(head);
          }
          
          pmath_unref(item);
        }
        
        pmath_unref(lbl);
        pmath_unref(expr);
        
        return result;
      }
    }
   
   NEXT_STEP:
    if(pmath_aborting())
      break;
  }
  
  if(i == len){ // tail recursion (there was no Label so we cannot "Goto" backwards)
    pmath_unref(result);
    result = pmath_expr_get_item(expr, len);
  }
  
  pmath_unref(expr);
  return result;
}
