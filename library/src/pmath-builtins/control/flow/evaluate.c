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
  if(pmath_instance_of(*in_out, PMATH_TYPE_EXPRESSION)){
    pmath_t head = pmath_expr_get_item((pmath_expr_t)*in_out, 0);
    pmath_unref(head);
    
    if(head == PMATH_SYMBOL_CONTINUE 
    || head == PMATH_SYMBOL_BREAK){
      pmath_bool_t do_break = head == PMATH_SYMBOL_BREAK;
      
      pmath_t counter = pmath_expr_get_item((pmath_expr_t)*in_out, 1);
      if(pmath_instance_of(counter, PMATH_TYPE_INTEGER)
      && pmath_compare(counter, PMATH_NUMBER_ONE) > 0){
        do_break = TRUE;
        counter = pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_PLUS), 2,
          counter,
          pmath_ref(PMATH_NUMBER_MINUSONE));
        *in_out = pmath_expr_set_item((pmath_expr_t)*in_out, 1, counter);
      }
      else{
        pmath_unref(counter);
        pmath_unref(*in_out);
        *in_out = NULL;
      }
      
      return do_break;
    }

    if(head == PMATH_SYMBOL_RETURN || head == PMATH_SYMBOL_GOTO)
      return TRUE;
  }

  pmath_unref(*in_out);
  *in_out = NULL;
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
  
  if(pmath_instance_of(reltime_obj, PMATH_TYPE_NUMBER))
    seconds = pmath_number_get_d(reltime_obj);
  
  pmath_unref(reltime_obj);
  if(seconds <= 0){
    pmath_message(NULL, "invtim", 1, pmath_expr_get_item(expr, 2));
    return expr;
  }
  
  mq = pmath_thread_get_queue();
  pmath_thread_send_delayed(mq, pmath_expr_get_item(expr, 1), seconds);
  pmath_unref(mq);
  pmath_unref(expr);
  return NULL;
}

PMATH_PRIVATE pmath_t builtin_release(pmath_expr_t expr){
/* Release(expr)
 */
  pmath_expr_t result;

  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  result = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  if(pmath_instance_of(result, PMATH_TYPE_EXPRESSION)){
    pmath_t obj = pmath_expr_get_item(result, 0);
    pmath_unref(obj);
    
    if(obj == PMATH_SYMBOL_HOLD
    || obj == PMATH_SYMBOL_HOLDCOMPLETE){
      if(pmath_expr_length(result) == 1){
        obj = pmath_expr_get_item(result, 1);
        pmath_unref(result);
        return obj;
      }
      
      return pmath_expr_set_item(
        result, 0, 
        pmath_ref(PMATH_SYMBOL_SEQUENCE));
    }
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
    return NULL;
  }
  
  have_label = FALSE;
  result = NULL;
  
  for(i = 1;i < len || (i==len && have_label);++i){
    pmath_unref(result);
    result = pmath_evaluate(pmath_expr_get_item(expr, i));
    
    if(pmath_instance_of(result, PMATH_TYPE_EXPRESSION)){
      pmath_t head = pmath_expr_get_item(result, 0);
      pmath_unref(head);
      
      if(head == PMATH_SYMBOL_BREAK
      || head == PMATH_SYMBOL_CONTINUE
      || head == PMATH_SYMBOL_RETURN){
        pmath_unref(expr);
        return result;
      }
      
      if(head == PMATH_SYMBOL_LABEL){
        have_label = TRUE;
      }
      else if(head == PMATH_SYMBOL_GOTO && pmath_expr_length(result) == 1){
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
              result = NULL;
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
