#include <pmath-util/evaluation.h>
#include <pmath-core/symbols.h>

#include <assert.h>
#include <string.h>

#include <pmath-util/debug.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/concurrency/threads-private.h>

#include <pmath-language/patterns-private.h>
#include <pmath-language/scanner.h>

#include <pmath-builtins/control/messages-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-language/patterns-private.h>

static void make_critical_message(pmath_t msg){ // msg will be freed
  if(pmath_instance_of(msg, PMATH_TYPE_SYMBOL)){
    PMATH_RUN_ARGS("Internal`IsCriticalMessage(HoldPattern(MessageName(`1`, ~))):= True", "(o)", msg);
  }
  else{
    pmath_t sym = pmath_expr_get_item(msg, 1);
    pmath_unref(sym);
    
    if(sym == PMATH_SYMBOL_GENERAL)
      msg = pmath_expr_set_item(msg, 1, pmath_ref(_pmath_object_singlematch));
    
    PMATH_RUN_ARGS("Internal`IsCriticalMessage(HoldPattern(`1`)):= True", "(o)", msg);
  }
}

PMATH_PRIVATE pmath_t builtin_try(pmath_expr_t expr){
/* Try(body, failexpr, messages)
   Try(body, failexpr)
   Try(body)
 */
  pmath_t         body;
  pmath_t         failexpr;
  pmath_t         messages;
  pmath_t         old_downrules;
  pmath_t         exception;
  pmath_thread_t  thread;
  uint8_t         old_critical_messages;
  size_t          exprlen;
  size_t          i;
  
  exprlen = pmath_expr_length(expr);

  if(exprlen < 1 || exprlen > 3){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 3);
    return expr;
  }
  
  thread = pmath_thread_get_current();
  
  if(!thread)
    return expr;
  
  if(exprlen == 3){
    messages = pmath_expr_get_item(expr, 3);
    
    if(pmath_instance_of(messages, PMATH_TYPE_SYMBOL)){
      messages = pmath_build_value("(o)", messages);
    }
    
    if(!pmath_instance_of(messages, PMATH_TYPE_EXPRESSION)){
      pmath_message(NULL, "nomsgs", 1, messages);
      return expr;
    }
    
    if(!_pmath_is_valid_messagename(messages)){
      pmath_t item = pmath_expr_get_item(messages, 0);
      pmath_unref(item);
      
      if(item != PMATH_SYMBOL_LIST){
        pmath_message(NULL, "nomsgs", 1, messages);
        return expr;
      }
      
      for(i = pmath_expr_length(messages);i > 0;--i){
        item = pmath_expr_get_item(messages, i);
        
        if(!pmath_instance_of(item, PMATH_TYPE_SYMBOL)
        && !_pmath_is_valid_messagename(item)){
          pmath_unref(item);
          pmath_message(NULL, "nomsgs", 1, messages);
          return expr;
        }
        
        pmath_unref(item);
      }
    }
    
    failexpr = pmath_expr_get_item(expr, 2);
  }
  else if(exprlen == 2){
    messages = NULL;
    failexpr = pmath_expr_get_item(expr, 2);
  }
  else{
    messages = NULL;
    failexpr = pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  body     = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  old_downrules = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_DOWNRULES), 1,
      pmath_ref(PMATH_SYMBOL_INTERNAL_ISCRITICALMESSAGE)));
  
  if(exprlen == 3){
    if(_pmath_is_valid_messagename(messages)){
      make_critical_message(pmath_ref(messages));
    }
    else{
      for(i = pmath_expr_length(messages);i > 0;--i){
        make_critical_message(pmath_expr_get_item(messages, i));
      }
    }
  }
  else{
    PMATH_RUN("Internal`IsCriticalMessage(~):= True");
  }
  
  old_critical_messages = thread->critical_messages;
  thread->critical_messages = TRUE;
  
  body = pmath_evaluate(body);
  
  exception = _pmath_thread_catch(thread);
  if(exception != PMATH_UNDEFINED){
    if(_pmath_is_valid_messagename(exception)){
      if(exprlen < 3 || pmath_equals(exception, messages)){
        pmath_unref(body);
        pmath_unref(exception);
        body = failexpr;
        failexpr = NULL;
        exception = PMATH_UNDEFINED;
      }
      else if(!_pmath_is_valid_messagename(messages)){ // list of messages
        for(i = pmath_expr_length(messages);i > 0;--i){
          pmath_t item;
          
          item = pmath_expr_get_item(messages, i);
          if(pmath_equals(exception, item)){
            pmath_unref(item);
            pmath_unref(body);
            pmath_unref(exception);
            body = failexpr;
            failexpr = NULL;
            exception = PMATH_UNDEFINED;
            break;
          }
          
          pmath_unref(item);
        }
      }
    }
  }
  
  thread->critical_messages = old_critical_messages;
  
  pmath_unref(messages);
  pmath_unref(failexpr);
  
  PMATH_RUN_ARGS("DownRules(Internal`IsCriticalMessage):= `1`", "(o)", old_downrules);
  
  if(exception != PMATH_UNDEFINED)
    _pmath_thread_throw(thread, exception);
    
  return body;
}
